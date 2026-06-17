/// @file
/// @brief Anthropic Claude streaming provider — POSTs to /v1/messages
///        with `stream: true` and feeds SSE events to a coroutine
///        generator of ProviderDelta.
///
/// Implements euxis::runtime::IStreamingProvider. The interface lives
/// behind EUXIS_HAS_STD_GENERATOR (set by streaming.hpp when libc++ /
/// libstdc++ ships `<generator>`); the entire class is therefore
/// gated by the same macro. On toolchains without `<generator>` this
/// header still parses — only the IStreamingProvider symbol is absent
/// — so consumers can include it unconditionally and check the macro
/// before constructing.
///
/// Wire format
/// -----------
/// POST `${base_url}/v1/messages`
///   x-api-key:         <api_key>
///   anthropic-version: 2023-06-01
///   content-type:      application/json
///   accept:            text/event-stream
///
/// Body:
///   {
///     "model":      "claude-...",
///     "max_tokens": 1024,
///     "stream":     true,
///     "messages":   [{"role": "user", "content": "<prompt>"}]
///   }
///
/// Response: text/event-stream with one event per SSE record:
///   - message_start         (ignored)
///   - content_block_start   (records tool_use id + name)
///   - content_block_delta   (text_delta -> TextFragment;
///                            input_json_delta -> ToolCallPartial)
///   - content_block_stop    (ignored)
///   - message_delta         (usage -> Usage)
///   - message_stop          (no delta; generator ends naturally)
///
/// Reference: https://docs.anthropic.com/en/api/messages-streaming
///
/// Implementation note — buffered streaming.
/// httplib's HTTP call is synchronous, so the receiver parses SSE
/// events as bytes arrive and pushes ProviderDelta records to an
/// internal buffer. The generator yields from that buffer once the
/// HTTP call returns. The API contract (std::generator<ProviderDelta>
/// consumed by a range-for) is identical to a true async pump; a
/// future commit can replace the buffered impl with a producer /
/// consumer queue without changing any caller.

#pragma once

#include <chrono>
#include <string>

#include "euxis/runtime/streaming.hpp"  // ProviderDelta, IStreamingProvider, EUXIS_HAS_STD_GENERATOR

namespace euxis::inference {

struct ClaudeStreamingConfig {
    /// Verifier identity (informational; matches the
    /// HttpVerifierConfig.id convention from libs/ensemble).
    std::string id{"claude"};

    /// Provider-specific model name.
    std::string model{"claude-sonnet-4-6"};

    /// Override base URL; empty falls back to Anthropic production.
    std::string base_url{"https://api.anthropic.com"};

    /// Explicit API key; empty falls back to ANTHROPIC_API_KEY.
    std::string api_key;

    /// Env-var name for the fallback API-key lookup.
    std::string env_var_name{"ANTHROPIC_API_KEY"};

    /// Per-stream wall-time budget.
    std::chrono::milliseconds timeout{std::chrono::seconds{60}};

    /// max_tokens forwarded to the Anthropic API.
    int max_tokens{1024};
};

#if defined(EUXIS_HAS_STD_GENERATOR)

class ClaudeStreamingProvider final : public euxis::runtime::IStreamingProvider {
public:
    explicit ClaudeStreamingProvider(ClaudeStreamingConfig cfg = {});

    [[nodiscard]] auto execute_stream(const std::string& model,
                                      const std::string& prompt,
                                      int timeout_ms = 30'000)
        -> std::generator<euxis::runtime::ProviderDelta> override;

    [[nodiscard]] auto config() const noexcept -> const ClaudeStreamingConfig& {
        return cfg_;
    }

private:
    ClaudeStreamingConfig cfg_;
    std::string resolved_key_;
};

#endif // EUXIS_HAS_STD_GENERATOR

} // namespace euxis::inference
