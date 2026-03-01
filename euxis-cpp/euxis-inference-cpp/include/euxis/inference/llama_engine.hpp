#pragma once

#include <memory>
#include <string_view>

#include "config.hpp"
#include "engine.hpp"

namespace euxis::inference {

/// llama.cpp-backed inference engine (PIMPL).
///
/// Connects to llama-server's OpenAI-compatible HTTP API at /v1/chat/completions.
/// Configure host/port via LLAMA_SERVER_HOST and LLAMA_SERVER_PORT env vars
/// (defaults to 127.0.0.1:8080).
class LlamaEngine final : public InferenceEngine {
public:
    explicit LlamaEngine(const LocalModelConfig& config);
    ~LlamaEngine() override;

    LlamaEngine(const LlamaEngine&) = delete;
    LlamaEngine& operator=(const LlamaEngine&) = delete;

    LlamaEngine(LlamaEngine&&) noexcept;
    LlamaEngine& operator=(LlamaEngine&&) noexcept;

    [[nodiscard]] auto generate(std::string_view prompt,
                                uint32_t max_tokens = 512)
        -> std::expected<InferenceResult, std::string> override;

    [[nodiscard]] auto supports_model(std::string_view name) -> bool override;
    [[nodiscard]] auto health() -> nlohmann::json override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace euxis::inference
