#include "euxis/ensemble/providers/openai.hpp"

#include <nlohmann/json.hpp>

namespace euxis::ensemble::providers {

auto default_openai_config() -> HttpVerifierConfig {
    HttpVerifierConfig cfg;
    cfg.id           = "openai";
    cfg.model        = "gpt-4o";
    cfg.base_url     = "https://api.openai.com";
    cfg.env_var_name = "OPENAI_API_KEY";
    return cfg;
}

OpenAIVerifier::OpenAIVerifier(HttpVerifierConfig cfg)
    : HttpVerifier{std::move(cfg)} {}

auto OpenAIVerifier::endpoint_path() const -> std::string {
    return "/v1/chat/completions";
}

auto OpenAIVerifier::request_headers() const
    -> std::vector<std::pair<std::string, std::string>> {
    return {
        {"Authorization", "Bearer " + api_key()},
    };
}

auto OpenAIVerifier::build_request_body(const std::string& prompt) const
    -> std::string {
    nlohmann::json body;
    body["model"]    = cfg_.model;
    body["messages"] = nlohmann::json::array({
        {{"role", "user"}, {"content", prompt}},
    });
    return body.dump();
}

auto OpenAIVerifier::extract_text(const std::string& body) const
    -> std::string {
    try {
        auto j = nlohmann::json::parse(body);
        if (!j.contains("choices") || !j["choices"].is_array() ||
            j["choices"].empty()) return {};
        const auto& first = j["choices"].front();
        if (!first.contains("message")) return {};
        return first["message"].value("content", std::string{});
    } catch (const std::exception&) {
        return {};
    }
}

} // namespace euxis::ensemble::providers
