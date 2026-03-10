/// @file
/// @brief Local LLM inference engine using Ollama.
#pragma once

#include <expected>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "llama_engine.hpp"

namespace euxis::inference {

/// @brief Inference engine that communicates with a local Ollama server.
class OllamaEngine final : public InferenceEngine {
public:
    /// @brief Construct engine with server coordinates.
    explicit OllamaEngine(std::string host = "localhost", uint16_t port = 11434);

    /// @brief Generate text completion using Ollama.
    auto generate(std::string_view prompt,
                  uint32_t max_tokens = 512)
        -> std::expected<InferenceResult, std::string> override;

    [[nodiscard]] auto supports_model(std::string_view name) -> bool override;
    [[nodiscard]] auto health() -> nlohmann::json override;

private:
    std::string host_;
    uint16_t port_;

    auto make_request(const std::string& endpoint,
                      const nlohmann::json& body) -> std::expected<nlohmann::json, std::string>;
};

} // namespace euxis::inference
