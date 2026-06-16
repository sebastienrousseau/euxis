/// @file
/// @brief Google Gemini verifier — generateContent endpoint.
///
/// Wire format
/// -----------
/// POST `${base_url}/v1beta/models/<model>:generateContent?key=<api_key>`
///   Content-Type: application/json
///
/// Body:
///   {
///     "contents": [{"role": "user",
///                    "parts": [{"text": "<prompt>"}]}]
///   }
///
/// Response:
///   {
///     "candidates": [{"content":
///                      {"parts": [{"text": "<assistant reply>"}]}}],
///     ...
///   }
///
/// Gemini's auth scheme is a query parameter, not a header — so
/// the endpoint path itself carries the key (built per-vote rather
/// than baked into `endpoint_path()`).
///
/// Reference: https://ai.google.dev/api/generate-content
#pragma once

#include "euxis/ensemble/providers/http_verifier.hpp"

namespace euxis::ensemble::providers {

[[nodiscard]] auto default_gemini_config() -> HttpVerifierConfig;

class GeminiVerifier final : public HttpVerifier {
public:
    explicit GeminiVerifier(HttpVerifierConfig cfg = default_gemini_config());

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
