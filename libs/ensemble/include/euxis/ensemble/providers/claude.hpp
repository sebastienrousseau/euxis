/// @file
/// @brief Anthropic Claude verifier — HTTP transport against the
///        Messages API.
///
/// Wire format
/// -----------
/// POST `${base_url}/v1/messages`
///   x-api-key:         <api_key>
///   anthropic-version: 2023-06-01
///   content-type:      application/json
///
/// Body (Claude Messages API v1):
///   {
///     "model":      "claude-...",
///     "max_tokens": <int>,
///     "messages":   [{"role": "user", "content": "<prompt>"}]
///   }
///
/// Response:
///   {
///     "content": [{"type": "text", "text": "<assistant reply>"}],
///     "stop_reason": "end_turn",
///     ...
///   }
///
/// Reference: https://docs.anthropic.com/en/api/messages
#pragma once

#include "euxis/ensemble/providers/http_verifier.hpp"

namespace euxis::ensemble::providers {

[[nodiscard]] auto default_claude_config() -> HttpVerifierConfig;

class ClaudeVerifier final : public HttpVerifier {
public:
    explicit ClaudeVerifier(HttpVerifierConfig cfg = default_claude_config());

protected:
    [[nodiscard]] auto build_request_body(const std::string& prompt) const
        -> std::string override;
    [[nodiscard]] auto extract_text(const std::string& body) const
        -> std::string override;
    [[nodiscard]] auto endpoint_path() const -> std::string override;
    [[nodiscard]] auto request_headers() const
        -> std::vector<std::pair<std::string, std::string>> override;
};

} // namespace euxis::ensemble::providers
