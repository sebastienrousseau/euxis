#include "euxis/ensemble/providers/http_verifier.hpp"

#include "euxis/ensemble/response.hpp"

#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <regex>
#include <string>
#include <thread>

namespace euxis::ensemble::providers {

namespace {

/// Split a base URL like "https://api.anthropic.com" into host
/// (with scheme) and a (possibly empty) prefix. httplib::Client
/// wants the scheme+host as one string and the path separately.
struct UrlParts {
    std::string scheme_host;  ///< "https://api.anthropic.com"
    std::string path_prefix;  ///< "" for production; "/test" for mock
};

auto split_url(const std::string& url) -> UrlParts {
    // Regex over "^(https?://[^/]+)(/.*)?$". Trailing slashes are
    // tolerated so callers can specify either form.
    static const std::regex re{R"(^(https?://[^/]+)(.*)$)"};
    std::smatch m;
    if (std::regex_match(url, m, re)) {
        UrlParts p{m[1].str(), m[2].str()};
        if (!p.path_prefix.empty() && p.path_prefix.back() == '/') {
            p.path_prefix.pop_back();
        }
        return p;
    }
    // Best-effort fallback: treat the whole input as the host.
    return UrlParts{url, ""};
}

/// Read env var if explicit key is empty. Empty-string return means
/// neither path resolved — the vote will short-circuit.
auto resolve_api_key(const HttpVerifierConfig& cfg) -> std::string {
    if (!cfg.api_key.empty()) return cfg.api_key;
    if (cfg.env_var_name.empty()) return {};
    const char* raw = std::getenv(cfg.env_var_name.c_str()); // NOLINT(concurrency-mt-unsafe)
    return raw != nullptr ? std::string{raw} : std::string{};
}

/// Exponential backoff for retryable HTTP statuses. Capped at 4s
/// so total wait stays bounded.
auto backoff(int attempt) -> std::chrono::milliseconds {
    using namespace std::chrono;
    auto base = milliseconds{250};
    auto scaled = base * (1 << std::min(attempt, 4));
    return scaled > seconds{4} ? milliseconds{seconds{4}} : scaled;
}

} // namespace

HttpVerifier::HttpVerifier(HttpVerifierConfig cfg)
    : cfg_{std::move(cfg)}, resolved_key_{resolve_api_key(cfg_)} {}

auto HttpVerifier::provider_id() const noexcept -> std::string {
    return cfg_.id;
}

auto HttpVerifier::api_key() const noexcept -> const std::string& {
    return resolved_key_;
}

namespace {

/// Construct a "verifier was unable to vote" VoteOutcome carrying
/// the supplied rationale. The runner treats a zero-confidence
/// `true_positive == false` as an unknown vote when computing
/// quorum, so this shape doesn't accidentally tip the result
/// either way.
auto unable_to_vote(const std::string& provider_id, std::string reason)
    -> VoteOutcome {
    VoteOutcome out;
    out.vote.provider      = provider_id;
    out.vote.true_positive = false;
    out.vote.confidence    = 0.0F;
    out.rationale          = std::move(reason);
    return out;
}

} // namespace

auto HttpVerifier::vote(const VoteRequest& req) -> VoteOutcome {
    if (resolved_key_.empty()) {
        return unable_to_vote(cfg_.id,
            "auth missing: set " + cfg_.env_var_name);
    }
    if (cfg_.base_url.empty()) {
        return unable_to_vote(cfg_.id, "base_url not configured");
    }

    auto prompt = build_prompt(req, cfg_.prompt);
    auto body   = build_request_body(prompt);
    auto parts  = split_url(cfg_.base_url);
    auto path   = parts.path_prefix + endpoint_path();

    httplib::Client cli{parts.scheme_host};
    cli.set_connection_timeout(cfg_.timeout);
    cli.set_read_timeout(cfg_.timeout);
    cli.set_write_timeout(cfg_.timeout);
    cli.enable_server_certificate_verification(true);

    httplib::Headers headers{{"Content-Type", "application/json"}};
    for (const auto& [k, v] : request_headers()) {
        headers.emplace(k, v);
    }

    httplib::Result rsp;
    int attempt = 0;
    for (; attempt <= cfg_.max_retries; ++attempt) {
        rsp = cli.Post(path, headers, body, "application/json");
        if (rsp && rsp->status < 500 && rsp->status != 429) break;
        if (attempt == cfg_.max_retries) break;
        std::this_thread::sleep_for(backoff(attempt));
    }

    if (!rsp) {
        return unable_to_vote(cfg_.id,
            "transport error: " + httplib::to_string(rsp.error()));
    }
    if (rsp->status >= 400) {
        return unable_to_vote(cfg_.id,
            "http " + std::to_string(rsp->status));
    }

    auto text = extract_text(rsp->body);
    if (text.empty()) {
        return unable_to_vote(cfg_.id, "empty assistant text");
    }

    // Reuse the existing response parser so the assistant is held
    // to the same JSON contract regardless of provider.
    auto parsed = parse_response(text, cfg_.id);
    if (!parsed) {
        return unable_to_vote(cfg_.id,
            "response parse: " + parsed.error().message);
    }
    return *parsed;
}

} // namespace euxis::ensemble::providers
