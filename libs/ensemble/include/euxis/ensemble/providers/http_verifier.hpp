/// @file
/// @brief HTTP-backed Verifier base class.
///
/// Concrete LLM providers (Claude, OpenAI, Gemini, ...) inherit
/// from `HttpVerifier` and supply provider-specific request shape
/// + auth headers + response extraction. The base class handles:
///
///   - prompt synthesis via `build_prompt()` (libs/ensemble/prompt)
///   - HTTP POST with timeout + bounded retry on 5xx / 429
///   - response parsing via `parse_response()` (libs/ensemble/response)
///   - API-key resolution from explicit config OR an env-var
///     fallback (so CI can authenticate via the standard
///     `OPENAI_API_KEY` / `ANTHROPIC_API_KEY` / `GEMINI_API_KEY`
///     conventions without leaking secrets in command lines).
///
/// Concurrency: each call to `vote()` constructs its own
/// `httplib::Client`, so HttpVerifier instances are safe to share
/// across threads — the ensemble runner can fan out one request
/// per provider in parallel without further synchronisation.
#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "euxis/ensemble/prompt.hpp"
#include "euxis/ensemble/verifier.hpp"

namespace euxis::ensemble::providers {

struct HttpVerifierConfig {
    /// Verifier identity threaded into every VoteOutcome.
    std::string id;

    /// Provider-specific model name (e.g. "claude-sonnet-4-6",
    /// "gpt-4o", "gemini-2.5-pro").
    std::string model;

    /// Override base URL — empty falls back to the provider's
    /// production endpoint. Mock-server tests set this to
    /// `http://localhost:<port>` so the same code path runs
    /// without touching the public internet.
    std::string base_url;

    /// Explicit API key. When empty the base class reads
    /// `env_var_name` from the process environment and uses that
    /// instead. Either path must succeed before vote() will issue
    /// a request — an empty key after both lookups yields a
    /// VoteOutcome::Unknown vote with an "auth missing" reason.
    std::string api_key;

    /// Env-var name to consult when `api_key` is empty. Each
    /// provider supplies a sensible default
    /// (CLAUDE_API_KEY / OPENAI_API_KEY / GEMINI_API_KEY).
    std::string env_var_name;

    /// Per-request wall-time budget. Includes connect + read.
    std::chrono::milliseconds timeout{std::chrono::seconds{30}};

    /// Retry count on 5xx / 429. Each retry waits an
    /// exponentially-growing backoff capped at 4s.
    int max_retries{3};

    /// Prompt-builder configuration. Controls the snippet-byte
    /// cap and the reviewer persona.
    PromptConfig prompt;
};

class HttpVerifier : public Verifier {
public:
    explicit HttpVerifier(HttpVerifierConfig cfg);

    [[nodiscard]] auto vote(const VoteRequest& req) -> VoteOutcome override;
    [[nodiscard]] auto provider_id() const noexcept -> std::string override;

protected:
    /// Build the provider-specific JSON body from the assembled
    /// prompt text. Implemented by each concrete provider.
    [[nodiscard]] virtual auto build_request_body(const std::string& prompt) const
        -> std::string = 0;

    /// Extract the assistant text from the provider's JSON response.
    /// Returning an empty string makes the runner treat the vote as
    /// VoteOutcome::Unknown.
    [[nodiscard]] virtual auto extract_text(const std::string& body) const
        -> std::string = 0;

    /// Endpoint path appended to `base_url`. E.g. "/v1/messages"
    /// for Claude, "/v1/chat/completions" for OpenAI.
    [[nodiscard]] virtual auto endpoint_path() const -> std::string = 0;

    /// Provider-specific request headers (auth, version, etc.).
    /// `Content-Type: application/json` is added by the base
    /// class so each provider doesn't have to remember.
    [[nodiscard]] virtual auto request_headers() const
        -> std::vector<std::pair<std::string, std::string>> = 0;

    /// Resolved API key — explicit config beats env var.
    [[nodiscard]] auto api_key() const noexcept -> const std::string&;

    HttpVerifierConfig cfg_;

private:
    /// Cached after construction so multiple votes don't repeat
    /// the env lookup. Mutable would be cleaner but the test that
    /// rewires env at runtime would then be impossible.
    std::string resolved_key_;
};

} // namespace euxis::ensemble::providers
