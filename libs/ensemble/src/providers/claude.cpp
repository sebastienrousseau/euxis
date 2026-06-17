#include "euxis/ensemble/providers/claude.hpp"

#include <nlohmann/json.hpp>

namespace euxis::ensemble::providers {

auto default_claude_config() -> HttpVerifierConfig {
    HttpVerifierConfig cfg;
    cfg.id           = "claude";
    cfg.model        = "claude-sonnet-4-6";
    cfg.base_url     = "https://api.anthropic.com";
    cfg.env_var_name = "ANTHROPIC_API_KEY";
    return cfg;
}

ClaudeVerifier::ClaudeVerifier(HttpVerifierConfig cfg)
    : HttpVerifier{std::move(cfg)} {}

auto ClaudeVerifier::endpoint_path() const -> std::string {
    return "/v1/messages";
}

auto ClaudeVerifier::request_headers() const
    -> std::vector<std::pair<std::string, std::string>> {
    return {
        {"x-api-key",         api_key()},
        {"anthropic-version", "2023-06-01"},
    };
}

auto ClaudeVerifier::build_request_body(const std::string& prompt) const
    -> std::string {
    nlohmann::json body;
    body["model"]      = cfg_.model;
    body["max_tokens"] = 1024;
    body["messages"]   = nlohmann::json::array({
        {{"role", "user"}, {"content", prompt}},
    });
    return body.dump();
}

auto ClaudeVerifier::extract_text(const std::string& body) const
    -> std::string {
    try {
        auto j = nlohmann::json::parse(body);
        if (!j.contains("content") || !j["content"].is_array()) return {};
        std::string out;
        for (const auto& item : j["content"]) {
            if (item.is_object() && item.value("type", "") == "text") {
                out += item.value("text", std::string{});
            }
        }
        return out;
    } catch (const std::exception&) {
        return {};
    }
}

} // namespace euxis::ensemble::providers
