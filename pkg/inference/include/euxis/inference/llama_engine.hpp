/// @file
/// @brief Core inference engine interfaces and local Llama.cpp implementation.
#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "config.hpp"

namespace euxis::inference {

/// @brief metadata and content of a successful inference generation.
struct InferenceResult {
    std::string text;
    uint32_t tokens_generated;
    float tokens_per_second;
    std::string engine_name;
    std::string model_name;
};

/// @brief Abstract interface for LLM completion engines.
class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;

    /// @brief generate a completion from a prompt.
    virtual auto generate(std::string_view prompt,
                          uint32_t max_tokens = 512)
        -> std::expected<InferenceResult, std::string> = 0;

    /// @brief check if this engine can run a specific model.
    [[nodiscard]] virtual auto supports_model(std::string_view name) -> bool = 0;
    
    /// @brief get engine health and status.
    [[nodiscard]] virtual auto health() -> nlohmann::json = 0;
};

/// @brief Local inference engine powered by llama.cpp.
class LlamaEngine final : public InferenceEngine {
public:
    /// @brief Construct engine with local model configuration.
    explicit LlamaEngine(const LocalModelConfig& config);
    ~LlamaEngine() override;

    LlamaEngine(const LlamaEngine&) = delete;
    LlamaEngine& operator=(const LlamaEngine&) = delete;

    LlamaEngine(LlamaEngine&&) noexcept;
    LlamaEngine& operator=(LlamaEngine&&) noexcept;

    /// @brief Generate completion using local weights.
    auto generate(std::string_view prompt,
                  uint32_t max_tokens = 512)
        -> std::expected<InferenceResult, std::string> override;

    [[nodiscard]] auto supports_model(std::string_view name) -> bool override;
    [[nodiscard]] auto health() -> nlohmann::json override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace euxis::inference
