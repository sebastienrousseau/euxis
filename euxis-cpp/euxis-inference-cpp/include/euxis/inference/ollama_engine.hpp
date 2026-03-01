#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "engine.hpp"

namespace euxis::inference {

/// Ollama REST API inference engine.
///
/// Communicates with a running Ollama daemon over HTTP (default localhost:11434).
/// Uses cpp-httplib for all HTTP requests.
class OllamaEngine final : public InferenceEngine {
public:
    explicit OllamaEngine(std::string host = "localhost",
                          uint16_t port = 11434);

    [[nodiscard]] auto generate(std::string_view prompt,
                                uint32_t max_tokens = 512)
        -> std::expected<InferenceResult, std::string> override;

    [[nodiscard]] auto supports_model(std::string_view name) -> bool override;
    [[nodiscard]] auto health() -> nlohmann::json override;

private:
    std::string host_;
    uint16_t port_;

    /// Issue a POST/GET to the Ollama API and parse the JSON response.
    auto make_request(const std::string& endpoint,
                      const nlohmann::json& body)
        -> std::expected<nlohmann::json, std::string>;
};

} // namespace euxis::inference
