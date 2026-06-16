#include "euxis/ensemble/providers/gemini.hpp"

#include <nlohmann/json.hpp>

namespace euxis::ensemble::providers {

auto default_gemini_config() -> HttpVerifierConfig {
    HttpVerifierConfig cfg;
    cfg.id           = "gemini";
    cfg.model        = "gemini-2.5-pro";
    cfg.base_url     = "https://generativelanguage.googleapis.com";
    cfg.env_var_name = "GEMINI_API_KEY";
    return cfg;
}

GeminiVerifier::GeminiVerifier(HttpVerifierConfig cfg)
    : HttpVerifier{std::move(cfg)} {}

auto GeminiVerifier::endpoint_path() const -> std::string {
    // Gemini puts the model in the path and the API key in the
    // query string. The base class appends path_prefix from
    // base_url and assembles the full URL.
    return "/v1beta/models/" + cfg_.model + ":generateContent?key=" + api_key();
}

auto GeminiVerifier::request_headers() const
    -> std::vector<std::pair<std::string, std::string>> {
    // Gemini auth lives in the query string; no extra headers
    // beyond Content-Type (which the base class adds).
    return {};
}

auto GeminiVerifier::build_request_body(const std::string& prompt) const
    -> std::string {
    nlohmann::json body;
    body["contents"] = nlohmann::json::array({
        {{"role", "user"},
         {"parts", nlohmann::json::array({{{"text", prompt}}})}},
    });
    return body.dump();
}

auto GeminiVerifier::extract_text(const std::string& body) const
    -> std::string {
    try {
        auto j = nlohmann::json::parse(body);
        if (!j.contains("candidates") || !j["candidates"].is_array() ||
            j["candidates"].empty()) return {};
        const auto& first = j["candidates"].front();
        if (!first.contains("content")) return {};
        const auto& content = first["content"];
        if (!content.contains("parts") || !content["parts"].is_array()) return {};
        std::string out;
        for (const auto& part : content["parts"]) {
            if (part.is_object()) {
                out += part.value("text", std::string{});
            }
        }
        return out;
    } catch (const std::exception&) {
        return {};
    }
}

} // namespace euxis::ensemble::providers
