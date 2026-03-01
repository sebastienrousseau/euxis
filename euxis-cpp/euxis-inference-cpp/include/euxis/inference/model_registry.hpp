#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace euxis::inference {

/// Metadata for a discovered local model file.
struct ModelInfo {
    std::string name;
    std::filesystem::path path;
    size_t file_size;
    std::string sha256;
    bool verified;
};

/// Scans a directory for GGUF model files and verifies their integrity
/// using libsodium's crypto_hash_sha256.
class ModelRegistry {
public:
    explicit ModelRegistry(std::filesystem::path models_dir);

    /// Scan the models directory for *.gguf files and return their info.
    [[nodiscard]] auto discover() -> std::vector<ModelInfo>;

    /// Verify a model's SHA-256 checksum against ModelInfo::sha256.
    [[nodiscard]] auto verify(const ModelInfo& model) -> bool;

    /// Find a previously-discovered model by name.
    [[nodiscard]] auto find(std::string_view name)
        -> std::optional<ModelInfo>;

private:
    std::filesystem::path models_dir_;
    std::vector<ModelInfo> models_;

    /// Compute the hex-encoded SHA-256 hash of a file.
    auto compute_sha256(const std::filesystem::path& path) -> std::string;
};

} // namespace euxis::inference
