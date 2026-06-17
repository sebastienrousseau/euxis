/// @file
/// @brief OpenAI Chat Completions streaming provider — POSTs to
///        /v1/chat/completions with `stream: true` and feeds SSE
///        events to a coroutine generator of ProviderDelta.
///
/// Implements euxis::runtime::IStreamingProvider. Same toolchain
/// guard story as ClaudeStreamingProvider — the class is only
/// available under EUXIS_HAS_STD_GENERATOR (set by streaming.hpp).
///
/// Wire format
/// -----------
/// POST `${base_url}/v1/chat/completions`
///   Authorization:  Bearer <api_key>
///   content-type:   application/json
///   accept:         text/event-stream
///
/// Body:
///   {
///     "model":    "gpt-...",
///     "stream":   true,
///     "stream_options": {"include_usage": true},
///     "messages": [{"role":"user","content":"<prompt>"}]
///   }
///
/// Response: text/event-stream with `data: {...}\n\n` records and a
/// final `data: [DONE]\n\n` terminator. Each JSON payload has the
/// shape `{"choices": [{"delta": {...}, "finish_reason": ...}],
/// "usage": {...}}`. Text fragments arrive as choices[0].delta.content;
/// tool-call partials as choices[0].delta.tool_calls[].function with
/// the argument JSON streamed across many small chunks.
///
/// Reference: https://platform.openai.com/docs/api-reference/chat-streaming

#pragma once

#include <chrono>
#include <string>

#include "euxis/runtime/streaming.hpp"

namespace euxis::inference {

struct OpenAIStreamingConfig {
    std::string id{"openai"};
    std::string model{"gpt-4o"};
    std::string base_url{"https://api.openai.com"};
    std::string api_key;
    std::string env_var_name{"OPENAI_API_KEY"};
    std::chrono::milliseconds timeout{std::chrono::seconds{60}};
};

#if defined(EUXIS_HAS_STD_GENERATOR)

class OpenAIStreamingProvider final : public euxis::runtime::IStreamingProvider {
public:
    explicit OpenAIStreamingProvider(OpenAIStreamingConfig cfg = {});

    [[nodiscard]] auto execute_stream(const std::string& model,
                                      const std::string& prompt,
                                      int timeout_ms = 30'000)
        -> std::generator<euxis::runtime::ProviderDelta> override;

    [[nodiscard]] auto config() const noexcept -> const OpenAIStreamingConfig& {
        return cfg_;
    }

private:
    OpenAIStreamingConfig cfg_;
    std::string resolved_key_;
};

#endif // EUXIS_HAS_STD_GENERATOR

} // namespace euxis::inference
