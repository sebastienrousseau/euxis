#include "euxis/memory/store.hpp"

#include <sodium.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>

#include "euxis/crypto/aes_gcm.hpp"
#include "euxis/crypto/key_derivation.hpp"
#include "euxis/crypto/xchacha.hpp"

namespace euxis::memory {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

/// Hex-encode a span of bytes.
[[nodiscard]] auto hex_encode(std::span<const std::byte> data) -> std::string {
    const size_t hex_len = data.size() * 2 + 1;
    std::string hex(hex_len, '\0');
    sodium_bin2hex(hex.data(), hex_len,
                   reinterpret_cast<const unsigned char*>(data.data()),
                   data.size());
    // sodium_bin2hex null-terminates; trim.
    hex.resize(std::strlen(hex.c_str()));
    return hex;
}

/// Compute SHA-256 of a string, returning the raw 32-byte digest.
[[nodiscard]] auto sha256(std::string_view input)
    -> std::array<std::byte, 32> {
    std::array<std::byte, 32> digest{};
    crypto_hash_sha256(
        reinterpret_cast<unsigned char*>(digest.data()),
        reinterpret_cast<const unsigned char*>(input.data()),
        input.size());
    return digest;
}

/// Base64-encode raw bytes using libsodium (VARIANT_ORIGINAL).
[[nodiscard]] auto base64_encode(std::span<const std::byte> data) -> std::string {
    const size_t b64_maxlen = sodium_base64_encoded_len(
        data.size(), sodium_base64_VARIANT_ORIGINAL);
    std::string b64(b64_maxlen, '\0');
    sodium_bin2base64(
        b64.data(), b64_maxlen,
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size(),
        sodium_base64_VARIANT_ORIGINAL);
    // Trim null terminator.
    b64.resize(std::strlen(b64.c_str()));
    return b64;
}

/// Base64-decode a string using libsodium (VARIANT_ORIGINAL).
[[nodiscard]] auto base64_decode(std::string_view encoded)
    -> std::vector<std::byte> {
    // Upper bound: decoded is at most 3/4 of encoded length.
    std::vector<std::byte> decoded(encoded.size());
    size_t decoded_len = 0;
    const int rc = sodium_base642bin(
        reinterpret_cast<unsigned char*>(decoded.data()),
        decoded.size(),
        encoded.data(),
        encoded.size(),
        nullptr,  // ignore characters
        &decoded_len,
        nullptr,  // end pointer
        sodium_base64_VARIANT_ORIGINAL);
    if (rc != 0) {
        return {};
    }
    decoded.resize(decoded_len);
    return decoded;
}

/// Generate a UTC ISO-8601 timestamp string.
[[nodiscard]] auto utc_now_iso8601() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
    gmtime_r(&time_t_now, &utc_tm);

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
                       utc_tm.tm_year + 1900, utc_tm.tm_mon + 1, utc_tm.tm_mday,
                       utc_tm.tm_hour, utc_tm.tm_min, utc_tm.tm_sec,
                       static_cast<int>(ms.count()));
}

/// Generate a random UUID-style hex string (32 hex chars from 16 random bytes).
[[nodiscard]] auto generate_entry_id() -> std::string {
    std::array<std::byte, 16> buf{};
    randombytes_buf(buf.data(), buf.size());
    return hex_encode(buf);
}

/// Convert a string_view to a span of const bytes.
[[nodiscard]] auto as_byte_span(std::string_view sv)
    -> std::span<const std::byte> {
    return {reinterpret_cast<const std::byte*>(sv.data()), sv.size()};
}

using AgentKey = std::span<const std::byte, 32>;

/// Seal content for `tier`: base64(nonce (24) || ciphertext+tag).
[[nodiscard]] auto seal(std::string_view content, AgentKey key, MemoryTier tier)
    -> std::expected<std::string, std::string> {
    const auto aad = tier_label(tier);
    auto box = euxis::crypto::xchacha_encrypt(as_byte_span(content), key,
                                              as_byte_span(aad));
    if (!box) {
        return std::unexpected("Encryption failed: " +
                               euxis::crypto::to_string(box.error()));
    }
    std::vector<std::byte> combined(box->nonce.begin(), box->nonce.end());
    combined.insert(combined.end(), box->ciphertext.begin(), box->ciphertext.end());
    return base64_encode(combined);
}

/// Split base64(prefix || ciphertext) into its nonce/IV and ciphertext.
template <std::size_t N>
[[nodiscard]] auto split_payload(const std::vector<std::byte>& raw)
    -> std::optional<std::pair<std::array<std::byte, N>, std::span<const std::byte>>> {
    if (raw.size() < N) {
        return std::nullopt;
    }
    std::array<std::byte, N> prefix{};
    std::memcpy(prefix.data(), raw.data(), N);
    return std::pair{prefix, std::span<const std::byte>(raw).subspan(N)};
}

/// Decrypt an entry with the AEAD its `alg` names.
[[nodiscard]] auto open_entry(const EncryptedMemoryEntry& entry, AgentKey key)
    -> std::expected<std::string, std::string> {
    const auto raw = base64_decode(entry.ciphertext_b64);
    const auto aad = as_byte_span(tier_label(entry.tier));
    std::expected<euxis::crypto::DecryptionResult, euxis::crypto::CryptoError> out =
        std::unexpected(euxis::crypto::CryptoError::DecryptionFailed);
    if (entry.alg == kAlgXChaCha) {
        auto parts = split_payload<euxis::crypto::kXChaChaNonceBytes>(raw);
        if (!parts) return std::unexpected(std::string("Invalid ciphertext: too short"));
        out = euxis::crypto::xchacha_decrypt(parts->second, key, parts->first, aad);
    } else if (entry.alg == kAlgLegacyAesGcm) {
        auto parts = split_payload<12>(raw);
        if (!parts) return std::unexpected(std::string("Invalid ciphertext: too short"));
        out = euxis::crypto::decrypt_aad(parts->second, key, parts->first, aad);
    } else {
        return std::unexpected("Unsupported algorithm: " + entry.alg);
    }
    if (!out) {
        return std::unexpected("Decryption failed: " +
                               euxis::crypto::to_string(out.error()));
    }
    return out->to_string();
}

/// Migrate one JSONL line: the re-sealed line for a legacy entry, nullopt
/// to keep the line as it is (current, unparsable or unknown entries).
[[nodiscard]] auto migrate_line(const std::string& line, AgentKey key)
    -> std::expected<std::optional<std::string>, std::string> {
    EncryptedMemoryEntry entry;
    try {
        entry = EncryptedMemoryEntry::from_json(nlohmann::json::parse(line));
    } catch (const std::exception&) {
        return std::optional<std::string>{};
    }
    if (entry.alg != kAlgLegacyAesGcm) {
        return std::optional<std::string>{};
    }
    auto plaintext = open_entry(entry, key);
    if (!plaintext) {
        return std::unexpected("entry " + entry.entry_id + ": " + plaintext.error());
    }
    auto sealed = seal(*plaintext, key, entry.tier);
    sodium_memzero(plaintext->data(), plaintext->size());
    if (!sealed) {
        return std::unexpected("entry " + entry.entry_id + ": " + sealed.error());
    }
    entry.ciphertext_b64 = std::move(*sealed);
    entry.alg = std::string{kAlgXChaCha};
    return std::optional<std::string>{entry.to_json().dump()};
}

/// Check whether a 32-byte key is all zeros.
[[nodiscard]] auto is_key_zeroed(const std::array<std::byte, 32>& key) -> bool {
    return sodium_is_zero(
        reinterpret_cast<const unsigned char*>(key.data()),
        key.size()) == 1;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
EncryptedMemoryStore::EncryptedMemoryStore(
    std::filesystem::path store_dir,
    std::span<const std::byte, 32> master_key,
    std::string agent_did)
    : agent_key_{}
    , store_path_{}
    , agent_did_(std::move(agent_did))
{
    derive_agent_key(master_key);

    // Store path: store_dir / sha256_hex(agent_did) / encrypted_memory.jsonl
    const auto did_hash = sha256(agent_did_);
    const auto did_hex = hex_encode(did_hash);
    store_path_ = store_dir / did_hex / "encrypted_memory.jsonl";

    // Ensure the directory exists.
    std::filesystem::create_directories(store_path_.parent_path());

    spdlog::debug("EncryptedMemoryStore initialised for agent {} at {}",
                  agent_did_, store_path_.string());
}

// ---------------------------------------------------------------------------
// derive_agent_key
// ---------------------------------------------------------------------------
void EncryptedMemoryStore::derive_agent_key(
    std::span<const std::byte, 32> master_key) {
    // Salt = first 16 bytes of SHA-256(agent_did).
    // crypto_pwhash requires exactly crypto_pwhash_SALTBYTES (16) bytes.
    const auto full_hash = sha256(agent_did_);
    std::array<std::byte, 16> salt{};
    std::memcpy(salt.data(), full_hash.data(), 16);

    // Use euxis-crypto-cpp key derivation with the master key as "password".
    auto derived = euxis::crypto::derive_key(
        master_key,
        std::span<const std::byte>(salt.data(), salt.size()),
        100'000,  // SENSITIVE opslimit
        32);

    if (!derived.has_value()) {
        spdlog::error("Agent key derivation failed for {}", agent_did_);
        sodium_memzero(agent_key_.data(), agent_key_.size());
        return;
    }

    std::memcpy(agent_key_.data(), derived->key.data(), 32);

    // Securely erase the intermediate derived key.
    sodium_memzero(derived->key.data(), derived->key.size());
}

// ---------------------------------------------------------------------------
// store
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::store(std::string_view content, MemoryTier tier)
    -> std::expected<EncryptedMemoryEntry, std::string> {

    assert(!content.empty() && "P10-R5: content must not be empty");
    assert(!agent_did_.empty() && "P10-R5: agent DID must be set");

    if (is_key_zeroed(agent_key_)) {
        return std::unexpected(std::string("Agent keys have been destroyed"));
    }

    // XChaCha20-Poly1305, AAD = tier label (runs on every CPU).
    auto sealed = seal(content, agent_key_, tier);
    if (!sealed) {
        return std::unexpected(sealed.error());
    }

    // Build the entry.
    EncryptedMemoryEntry entry;
    entry.entry_id       = generate_entry_id();
    entry.tier           = tier;
    entry.ciphertext_b64 = std::move(*sealed);
    entry.created_at     = utc_now_iso8601();
    entry.agent_did      = agent_did_;
    entry.alg            = std::string{kAlgXChaCha};

    // Persist.
    save_entry(entry);

    spdlog::debug("Stored memory entry {} (tier={}) for agent {}",
                  entry.entry_id, std::string(tier_label(tier)), agent_did_);

    return entry;
}

// ---------------------------------------------------------------------------
// retrieve
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::retrieve(std::string_view entry_id)
    -> std::expected<std::string, std::string> {

    assert(!entry_id.empty() && "P10-R5: entry_id must not be empty");
    assert(!agent_did_.empty() && "P10-R5: agent DID must be set");

    if (is_key_zeroed(agent_key_)) {
        return std::unexpected(std::string("Agent keys have been destroyed"));
    }

    const auto& all = entries();
    std::string id_str(entry_id);
    auto idx_it = cache_index_.find(id_str);
    if (idx_it == cache_index_.end()) {
        return std::unexpected(
            std::string("Entry not found: ") + id_str);
    }
    return open_entry(all[idx_it->second], agent_key_);
}

// ---------------------------------------------------------------------------
// retrieve_tier
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::retrieve_tier(MemoryTier tier, size_t limit)
    -> std::vector<std::string> {

    if (is_key_zeroed(agent_key_)) {
        return {};
    }

    const auto& entries = this->entries();
    std::vector<std::string> results;
    results.reserve(std::min(limit, entries.size()));

    for (const auto& entry : entries) {
        if (results.size() >= limit) break;
        if (entry.tier != tier) continue;

        if (auto content = open_entry(entry, agent_key_)) {
            results.push_back(std::move(*content));
        }
    }

    return results;
}

// ---------------------------------------------------------------------------
// destroy_agent_keys
// ---------------------------------------------------------------------------
void EncryptedMemoryStore::destroy_agent_keys() {
    sodium_memzero(agent_key_.data(), agent_key_.size());
    spdlog::info("Agent keys destroyed for {}", agent_did_);
}

// ---------------------------------------------------------------------------
// export_tier_encrypted
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::export_tier_encrypted(MemoryTier tier)
    -> std::vector<EncryptedMemoryEntry> {

    const auto& entries = this->entries();
    std::vector<EncryptedMemoryEntry> filtered;

    std::ranges::copy_if(entries, std::back_inserter(filtered),
        [tier](const auto& e) { return e.tier == tier; });

    return filtered;
}

// ---------------------------------------------------------------------------
// migrate_legacy_entries
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::migrate_legacy_entries()
    -> std::expected<std::size_t, std::string> {
    if (is_key_zeroed(agent_key_)) {
        return std::unexpected(std::string("Agent keys have been destroyed"));
    }
    std::ifstream in(store_path_);
    if (!in.is_open()) {
        return std::size_t{0}; // no store file yet: nothing to migrate
    }
    std::ostringstream rewritten;
    std::size_t migrated = 0;
    for (std::string line; std::getline(in, line);) {
        auto result = migrate_line(line, agent_key_);
        if (!result) {
            return std::unexpected("Migration aborted, store unchanged: " + result.error());
        }
        rewritten << result->value_or(line) << '\n';
        migrated += result->has_value() ? 1 : 0;
    }
    in.close();
    if (migrated == 0) {
        return std::size_t{0};
    }
    auto tmp = store_path_;
    tmp += ".migrating";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << rewritten.str();
        out.flush();
        if (!out) {
            std::filesystem::remove(tmp);
            return std::unexpected(std::string("Migration aborted: cannot write ") + tmp.string());
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, store_path_, ec); // atomic on POSIX
    if (ec) {
        std::filesystem::remove(tmp);
        return std::unexpected("Migration aborted: " + ec.message());
    }
    invalidate_cache();
    spdlog::info("Migrated {} memory entries to {} for {}", migrated,
                 std::string(kAlgXChaCha), agent_did_);
    return migrated;
}

// ---------------------------------------------------------------------------
// entries (cached access)
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::entries() const -> const std::vector<EncryptedMemoryEntry>& {
    if (!cache_valid_) {
        cache_ = load_entries_from_disk();
        cache_index_.clear();
        cache_index_.reserve(cache_.size());
        for (size_t i = 0; i < cache_.size(); ++i) {
            cache_index_[cache_[i].entry_id] = i;
        }
        cache_valid_ = true;
    }
    return cache_;
}

void EncryptedMemoryStore::invalidate_cache() {
    cache_valid_ = false;
}

// ---------------------------------------------------------------------------
// load_entries_from_disk
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::load_entries_from_disk() const -> std::vector<EncryptedMemoryEntry> {
    /// P10-R2: Maximum number of JSONL entries to read.
    constexpr size_t kMaxEntries = 100000;

    std::vector<EncryptedMemoryEntry> entries;

    if (!std::filesystem::exists(store_path_)) {
        return entries;
    }

    std::ifstream ifs(store_path_);
    if (!ifs.is_open()) {
        spdlog::warn("Failed to open store file: {}", store_path_.string());
        return entries;
    }

    std::string line;
    size_t count = 0;
    while (std::getline(ifs, line) && count < kMaxEntries) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            entries.push_back(EncryptedMemoryEntry::from_json(j));
            ++count;
        } catch (const std::exception& ex) {
            spdlog::warn("Skipping malformed JSONL line: {}", ex.what());
        }
    }

    assert(count <= kMaxEntries && "P10-R2: entry count bounded");
    return entries;
}

// ---------------------------------------------------------------------------
// save_entry
// ---------------------------------------------------------------------------
void EncryptedMemoryStore::save_entry(const EncryptedMemoryEntry& entry) {
    std::ofstream ofs(store_path_, std::ios::app);
    if (!ofs.is_open()) {
        spdlog::error("Failed to open store file for writing: {}",
                      store_path_.string());
        return;
    }
    ofs << entry.to_json().dump() << '\n';

    // Append to cache directly (no full re-read needed)
    if (cache_valid_) {
        cache_index_[entry.entry_id] = cache_.size();
        cache_.push_back(entry);
    }
}

} // namespace euxis::memory
