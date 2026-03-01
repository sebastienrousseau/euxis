#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::bridge {

struct ProvenanceEntry {
    std::string artifact_hash;   // SHA-256 hex
    std::string builder_id;
    std::string build_type;      // "https://slsa.dev/provenance/v1"
    std::string timestamp;
    std::vector<std::string> materials;  // Hashes of input materials
    nlohmann::json metadata;
};

class ProvenanceChain {
public:
    /// Compute SHA-256 of a file
    [[nodiscard]] static auto hash_file(const std::filesystem::path& path) -> std::string;

    /// Compute SHA-256 of a byte buffer
    [[nodiscard]] static auto hash_bytes(const std::string& data) -> std::string;

    /// Record provenance for an artifact
    auto record(const std::filesystem::path& artifact,
                const std::string& builder_id,
                const std::vector<std::string>& materials = {},
                nlohmann::json metadata = {}) -> ProvenanceEntry;

    /// Verify that a file matches a provenance entry
    [[nodiscard]] auto verify(const std::filesystem::path& artifact,
                              const ProvenanceEntry& entry) -> bool;

    /// Get the full provenance chain
    [[nodiscard]] auto chain() const -> const std::vector<ProvenanceEntry>&;

private:
    std::vector<ProvenanceEntry> chain_;
};

}  // namespace euxis::bridge
