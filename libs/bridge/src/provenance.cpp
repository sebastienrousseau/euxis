#include <euxis/bridge/provenance.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <sodium.h>

namespace euxis::bridge {

namespace {
auto now_iso8601() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);
    char buf[32];
    (void) std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

auto to_hex(const unsigned char* data, size_t len) -> std::string {
    std::ostringstream ss;
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(data[i]);
    }
    return ss.str();
}
}  // namespace

auto ProvenanceChain::hash_file(const std::filesystem::path& path) -> std::string {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);

    char buf[65536];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        crypto_hash_sha256_update(
            &state,
            reinterpret_cast<const unsigned char*>(buf),
            static_cast<size_t>(file.gcount())
        );
    }

    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256_final(&state, hash);

    return to_hex(hash, crypto_hash_sha256_BYTES);
}

auto ProvenanceChain::hash_bytes(const std::string& data) -> std::string {
    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(
        hash,
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size()
    );
    return to_hex(hash, crypto_hash_sha256_BYTES);
}

auto ProvenanceChain::record(const std::filesystem::path& artifact,
                             const std::string& builder_id,
                             const std::vector<std::string>& materials,
                             nlohmann::json metadata) -> ProvenanceEntry {
    ProvenanceEntry entry;
    entry.artifact_hash = hash_file(artifact);
    entry.builder_id = builder_id;
    entry.build_type = "https://slsa.dev/provenance/v1";
    entry.timestamp = now_iso8601();
    entry.materials = materials;
    entry.metadata = std::move(metadata);

    chain_.push_back(entry);
    return entry;
}

auto ProvenanceChain::verify(const std::filesystem::path& artifact,
                             const ProvenanceEntry& entry) -> bool {
    auto current_hash = hash_file(artifact);
    return !current_hash.empty() && current_hash == entry.artifact_hash;
}

auto ProvenanceChain::chain() const -> const std::vector<ProvenanceEntry>& {
    return chain_;
}

}  // namespace euxis::bridge
