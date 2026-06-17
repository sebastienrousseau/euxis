/// @file
/// @brief Google Gemini streaming provider — POSTs to
///        /v1beta/models/<model>:streamGenerateContent?alt=sse and
///        feeds SSE events to a coroutine generator of ProviderDelta.
///
/// Implements euxis::runtime::IStreamingProvider. Same toolchain guard
/// story as the other two streaming providers — the class is only
/// available under EUXIS_HAS_STD_GENERATOR.
///
/// Wire format
/// -----------
/// POST `${base_url}/v1beta/models/<model>:streamGenerateContent?alt=sse&key=<api_key>`
///   content-type: application/json
///   accept:       text/event-stream
///
/// Body:
///   {
///     "contents": [
///       {"role":"user", "parts":[{"text":"<prompt>"}]}
///     ]
///   }
///
/// Response: text/event-stream with `data: {...}\n\n` records. Each
/// JSON payload is a `GenerateContentResponse` shaped as
/// `{"candidates":[{"content":{"parts":[{"text":"..."} | {"functionCall":{...}}]}}],
///   "usageMetadata":{"promptTokenCount":..,"candidatesTokenCount":..}}`.
/// Unlike Claude / OpenAI, Gemini emits functionCall objects fully
/// formed — the JSON arrives complete in one SSE frame rather than
/// streamed across many partials. The provider therefore serialises
/// the args once and emits one ToolCallPartial carrying the full
/// JSON.
///
/// Reference: https://ai.google.dev/api/generate-content#method:-models.streamgeneratecontent

#pragma once

#include <chrono>
#include <string>

#include "euxis/runtime/streaming.hpp"

namespace euxis::inference {

struct GeminiStreamingConfig {
    std::string id{"gemini"};
    std::string model{"gemini-2.5-pro"};
    std::string base_url{"https://generativelanguage.googleapis.com"};
    std::string api_key;
    std::string env_var_name{"GEMINI_API_KEY"};
    std::chrono::milliseconds timeout{std::chrono::seconds{60}};
};

#if defined(EUXIS_HAS_STD_GENERATOR)

class GeminiStreamingProvider final : public euxis::runtime::IStreamingProvider {
public:
    explicit GeminiStreamingProvider(GeminiStreamingConfig cfg = {});

    [[nodiscard]] auto execute_stream(const std::string& model,
                                      const std::string& prompt,
                                      int timeout_ms = 30'000)
        -> std::generator<euxis::runtime::ProviderDelta> override;

    [[nodiscard]] auto config() const noexcept -> const GeminiStreamingConfig& {
        return cfg_;
    }

private:
    GeminiStreamingConfig cfg_;
    std::string resolved_key_;
};

#endif // EUXIS_HAS_STD_GENERATOR

} // namespace euxis::inference
