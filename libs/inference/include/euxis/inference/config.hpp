#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::inference {

/// Configuration for a locally-loaded GGUF model.
struct LocalModelConfig {
    std::string model_name;
    std::filesystem::path model_path;
    uint32_t context_size = 2048;
    uint32_t gpu_layers   = 0;
    uint32_t threads      = 4;
    float temperature     = 0.7f;
    float top_p           = 0.9f;
    std::string expected_sha256;

    /// Deserialise from JSON.
    [[nodiscard]] static auto from_json(const nlohmann::json& j)
        -> LocalModelConfig;

    /// Serialise to JSON.
    [[nodiscard]] nlohmann::json to_json() const;
};

} // namespace euxis::inference
