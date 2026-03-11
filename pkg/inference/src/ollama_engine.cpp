#include "euxis/inference/ollama_engine.hpp"

#include <chrono>
#include <format>

#include <spdlog/spdlog.h>

// cpp-httplib — single-header HTTP client
#include <httplib.h>

namespace euxis::inference {

// ---------------------------------------------------------------------------
// constructor
// ---------------------------------------------------------------------------
OllamaEngine::OllamaEngine(std::string host, uint16_t port)
    : host_(std::move(host)), port_(port) {}

// ---------------------------------------------------------------------------
// make_request — POST JSON to an Ollama endpoint
// ---------------------------------------------------------------------------
auto OllamaEngine::make_request(const std::string& endpoint,
                                const nlohmann::json& body)
    -> std::expected<nlohmann::json, std::string> {
    httplib::Client cli(host_, port_);
    cli.set_connection_timeout(5); // 5 seconds
    cli.set_read_timeout(120);     // 2 minutes for generation

    auto res = cli.Post(endpoint, body.dump(), "application/json");

    if (!res) {
        auto err = res.error();
        return std::unexpected(
            std::format("HTTP request to {} failed: {}", endpoint,
                        httplib::to_string(err)));
    }

    if (res->status != 200) {
        return std::unexpected(
            std::format("Ollama returned HTTP {}: {}", res->status,
                        res->body));
    }

    try {
        return nlohmann::json::parse(res->body);
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected(
            std::format("Failed to parse Ollama response: {}", e.what()));
    }
}

// ---------------------------------------------------------------------------
// generate
// ---------------------------------------------------------------------------
auto OllamaEngine::generate(std::string_view prompt,
                             uint32_t max_tokens)
    -> std::expected<InferenceResult, std::string> {
    nlohmann::json body = {
        {"model",  "llama3"},
        {"prompt", std::string(prompt)},
        {"stream", false},
        {"options", {
            {"num_predict", max_tokens},
        }},
    };

    auto result = make_request("/api/generate", body);
    if (!result) {
        return std::unexpected(result.error());
    }

    const auto& j = *result;
    InferenceResult ir;
    ir.engine_name = "ollama";
    ir.model_name  = j.value("model", "unknown");
    ir.text        = j.value("response", "");

    // Ollama returns eval_count and eval_duration (nanoseconds)
    ir.tokens_generated = j.value("eval_count", 0u);
    auto eval_ns = j.value("eval_duration", 0.0);
    if (eval_ns > 0.0) {
        ir.tokens_per_second =
            static_cast<float>(ir.tokens_generated) /
            (static_cast<float>(eval_ns) / 1.0e9f);
    } else {
        ir.tokens_per_second = 0.0f;
    }

    return ir;
}

// ---------------------------------------------------------------------------
// episodic_generate
// ---------------------------------------------------------------------------
auto OllamaEngine::episodic_generate(std::generator<euxis::runtime::SessionMessage> episodes,
                                     std::string_view system_prompt,
                                     uint32_t max_tokens)
    -> std::expected<InferenceResult, std::string> {
    
    std::string prompt = std::string(system_prompt) + "\n\n";
    for (auto msg : episodes) {
        prompt += std::format("[{}] {}\n", 
            (msg.role == euxis::runtime::Role::User ? "User" : "Assistant"),
            msg.content);
    }
    prompt += "\nAssistant: ";

    return generate(prompt, max_tokens);
}

// ---------------------------------------------------------------------------
// supports_model — ask Ollama for its model list
// ---------------------------------------------------------------------------
auto OllamaEngine::supports_model(std::string_view name) -> bool {
    httplib::Client cli(host_, port_);
    cli.set_connection_timeout(3);
    cli.set_read_timeout(5);

    auto res = cli.Get("/api/tags");
    if (!res || res->status != 200) {
        return false;
    }

    try {
        auto j = nlohmann::json::parse(res->body);
        if (!j.contains("models") || !j["models"].is_array()) {
            return false;
        }
        for (const auto& model : j["models"]) {
            auto model_name = model.value("name", "");
            // Ollama model names may include tag, e.g. "llama3:latest"
            if (model_name == name ||
                model_name.starts_with(std::string(name) + ":")) {
                return true;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// health — GET /api/tags
// ---------------------------------------------------------------------------
auto OllamaEngine::health() -> nlohmann::json {
    httplib::Client cli(host_, port_);
    cli.set_connection_timeout(3);
    cli.set_read_timeout(5);

    auto res = cli.Get("/api/tags");

    nlohmann::json status;
    status["engine"] = "ollama";
    status["host"]   = host_;
    status["port"]   = port_;

    if (!res) {
        status["status"] = "unreachable";
        status["error"]  = httplib::to_string(res.error());
        return status;
    }

    if (res->status != 200) {
        status["status"]    = "error";
        status["http_code"] = res->status;
        return status;
    }

    try {
        auto tags = nlohmann::json::parse(res->body);
        status["status"]  = "ok";
        status["models"]  = tags.value("models", nlohmann::json::array());
    } catch (...) {
        status["status"] = "parse_error";
    }

    return status;
}

} // namespace euxis::inference
