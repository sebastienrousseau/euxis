#include "euxis/inference/model_registry.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

#include <sodium.h>

namespace euxis::inference {

// ---------------------------------------------------------------------------
// constructor
// ---------------------------------------------------------------------------
ModelRegistry::ModelRegistry(std::filesystem::path models_dir)
    : models_dir_(std::move(models_dir)) {}

// ---------------------------------------------------------------------------
// discover — scan models_dir_ for *.gguf files
// ---------------------------------------------------------------------------
auto ModelRegistry::discover() -> std::vector<ModelInfo> {
    models_.clear();

    if (!std::filesystem::exists(models_dir_) ||
        !std::filesystem::is_directory(models_dir_)) {
        return models_;
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(models_dir_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".gguf") {
            continue;
        }

        ModelInfo info;
        info.name      = entry.path().stem().string();
        info.path      = entry.path();
        info.file_size = static_cast<size_t>(entry.file_size());
        info.sha256    = compute_sha256(entry.path());
        info.verified  = false;

        models_.push_back(std::move(info));
    }

    return models_;
}

// ---------------------------------------------------------------------------
// verify — recompute SHA-256 and compare against stored hash
// ---------------------------------------------------------------------------
auto ModelRegistry::verify(const ModelInfo& model) -> bool {
    if (!std::filesystem::exists(model.path)) {
        return false;
    }
    const auto hash = compute_sha256(model.path);
    return hash == model.sha256;
}

// ---------------------------------------------------------------------------
// find — look up a discovered model by name
// ---------------------------------------------------------------------------
auto ModelRegistry::find(std::string_view name)
    -> std::optional<ModelInfo> {
    for (const auto& m : models_) {
        if (m.name == name) {
            return m;
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// compute_sha256 — stream-hash a file with libsodium
// ---------------------------------------------------------------------------
auto ModelRegistry::compute_sha256(const std::filesystem::path& path)
    -> std::string {
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    constexpr size_t buf_size = 64ULL * 1024; // 64 KiB chunks
    std::vector<unsigned char> buf(buf_size);

    while (file.read(reinterpret_cast<char*>(buf.data()),
                     static_cast<std::streamsize>(buf_size))) {
        crypto_hash_sha256_update(
            &state, buf.data(),
            static_cast<unsigned long long>(file.gcount()));
    }
    // Handle the final partial read
    if (file.gcount() > 0) {
        crypto_hash_sha256_update(
            &state, buf.data(),
            static_cast<unsigned long long>(file.gcount()));
    }

    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256_final(&state, hash);

    // Hex-encode
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (size_t i = 0; i < crypto_hash_sha256_BYTES; ++i) {
        hex << std::setw(2) << static_cast<unsigned>(hash[i]);
    }
    return hex.str();
}

} // namespace euxis::inference
