/// @file
/// @brief OpenAI verifier — Chat Completions API.
///
/// Wire format
/// -----------
/// POST `${base_url}/v1/chat/completions`
///   Authorization: Bearer <api_key>
///   Content-Type:  application/json
///
/// Body:
///   {
///     "model":    "gpt-...",
///     "messages": [{"role": "user", "content": "<prompt>"}]
///   }
///
/// Response:
///   {
///     "choices": [{"message": {"role": "assistant",
///                              "content": "<assistant reply>"}}],
///     ...
///   }
///
/// Reference: https://platform.openai.com/docs/api-reference/chat
#pragma once

#include "euxis/ensemble/providers/http_verifier.hpp"

namespace euxis::ensemble::providers {

[[nodiscard]] auto default_openai_config() -> HttpVerifierConfig;

class OpenAIVerifier final : public HttpVerifier {
public:
    explicit OpenAIVerifier(HttpVerifierConfig cfg = default_openai_config());

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
