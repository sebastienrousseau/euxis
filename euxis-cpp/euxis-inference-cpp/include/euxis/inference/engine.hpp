#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace euxis::inference {

/// Result of a single inference call.
struct InferenceResult {
    std::string text;
    uint32_t tokens_generated;
    float tokens_per_second;
    std::string engine_name;
    std::string model_name;
};

/// Abstract interface for local inference engines.
class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;

    /// Generate text from a prompt using the underlying model.
    [[nodiscard]] virtual auto generate(std::string_view prompt,
                                        uint32_t max_tokens = 512)
        -> std::expected<InferenceResult, std::string> = 0;

    /// Check whether this engine can serve the given model name.
    [[nodiscard]] virtual auto supports_model(std::string_view name) -> bool = 0;

    /// Return a JSON health-check blob with engine status.
    [[nodiscard]] virtual auto health() -> nlohmann::json = 0;
};

} // namespace euxis::inference
