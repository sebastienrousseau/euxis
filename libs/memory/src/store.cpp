#include "euxis/memory/store.hpp"

#include <sodium.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "euxis/crypto/aes_gcm.hpp"
#include "euxis/crypto/key_derivation.hpp"

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

    // Get milliseconds
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
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

    // Convert content to bytes.
    const auto content_bytes = as_byte_span(content);

    // AAD is the tier label.
    const auto aad_str = tier_label(tier);
    const auto aad_bytes = as_byte_span(aad_str);

    // Encrypt with AAD.
    auto enc_result = euxis::crypto::encrypt_aad(
        content_bytes, agent_key_, aad_bytes);

    if (!enc_result.has_value()) {
        return std::unexpected(
            std::string("Encryption failed: ") +
            std::string(euxis::crypto::to_string(enc_result.error())));
    }

    // Combine IV (12 bytes) + ciphertext into a single buffer, then base64.
    std::vector<std::byte> combined(
        enc_result->iv.size() + enc_result->ciphertext.size());
    std::memcpy(combined.data(),
                enc_result->iv.data(),
                enc_result->iv.size());
    std::memcpy(combined.data() + enc_result->iv.size(),
                enc_result->ciphertext.data(),
                enc_result->ciphertext.size());

    const auto b64 = base64_encode(combined);

    // Build the entry.
    EncryptedMemoryEntry entry;
    entry.entry_id       = generate_entry_id();
    entry.tier           = tier;
    entry.ciphertext_b64 = b64;
    entry.created_at     = utc_now_iso8601();
    entry.agent_did      = agent_did_;

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

    auto entries = load_entries();
    auto it = std::ranges::find_if(entries, [&](const auto& e) {
        return e.entry_id == entry_id;
    });

    if (it == entries.end()) {
        return std::unexpected(
            std::string("Entry not found: ") + std::string(entry_id));
    }

    // Base64-decode.
    auto raw = base64_decode(it->ciphertext_b64);
    if (raw.size() < 12) {
        return std::unexpected(std::string("Invalid ciphertext: too short"));
    }

    // Split: first 12 bytes = IV, rest = ciphertext.
    std::array<std::byte, 12> iv{};
    std::memcpy(iv.data(), raw.data(), 12);
    std::span<const std::byte> ciphertext(raw.data() + 12, raw.size() - 12);

    // AAD is the tier label.
    const auto aad_str = tier_label(it->tier);
    const auto aad_bytes = as_byte_span(aad_str);

    // Decrypt.
    auto dec_result = euxis::crypto::decrypt_aad(
        ciphertext, agent_key_, iv, aad_bytes);

    if (!dec_result.has_value()) {
        return std::unexpected(
            std::string("Decryption failed: ") +
            std::string(euxis::crypto::to_string(dec_result.error())));
    }

    return dec_result->to_string();
}

// ---------------------------------------------------------------------------
// retrieve_tier
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::retrieve_tier(MemoryTier tier, size_t limit)
    -> std::vector<std::string> {

    if (is_key_zeroed(agent_key_)) {
        return {};
    }

    auto entries = load_entries();
    std::vector<std::string> results;
    results.reserve(std::min(limit, entries.size()));

    for (const auto& entry : entries) {
        if (results.size() >= limit) break;
        if (entry.tier != tier) continue;

        // Base64-decode.
        auto raw = base64_decode(entry.ciphertext_b64);
        if (raw.size() < 12) continue;

        std::array<std::byte, 12> iv{};
        std::memcpy(iv.data(), raw.data(), 12);
        std::span<const std::byte> ciphertext(raw.data() + 12, raw.size() - 12);

        const auto aad_str = tier_label(entry.tier);
        const auto aad_bytes = as_byte_span(aad_str);

        auto dec_result = euxis::crypto::decrypt_aad(
            ciphertext, agent_key_, iv, aad_bytes);

        if (dec_result.has_value()) {
            results.push_back(dec_result->to_string());
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

    auto entries = load_entries();
    std::vector<EncryptedMemoryEntry> filtered;

    std::ranges::copy_if(entries, std::back_inserter(filtered),
        [tier](const auto& e) { return e.tier == tier; });

    return filtered;
}

// ---------------------------------------------------------------------------
// load_entries
// ---------------------------------------------------------------------------
auto EncryptedMemoryStore::load_entries() -> std::vector<EncryptedMemoryEntry> {
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
}

} // namespace euxis::memory
