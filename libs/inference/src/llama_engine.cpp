#include "euxis/inference/llama_engine.hpp"

#include <chrono>
#include <cstdlib>
#include <format>

#include <spdlog/spdlog.h>

// cpp-httplib — single-header HTTP client
#include <httplib.h>

namespace euxis::inference {

// ---------------------------------------------------------------------------
// Impl — PIMPL internals (talks to llama-server over HTTP)
// ---------------------------------------------------------------------------
struct LlamaEngine::Impl {
    LocalModelConfig config;
    std::string host = "127.0.0.1";
    uint16_t port    = 8080;
    bool server_running = false;

    explicit Impl(const LocalModelConfig& cfg) : config(cfg) {
        // Read host/port overrides from environment
        if (const char* env_host = std::getenv("LLAMA_SERVER_HOST")) {
            host = env_host;
        }
        if (const char* env_port = std::getenv("LLAMA_SERVER_PORT")) {
            try {
                auto parsed = std::stoul(env_port);
                if (parsed > 0 && parsed <= 65535) {
                    port = static_cast<uint16_t>(parsed);
                }
            } catch (const std::exception&) {
                spdlog::warn("LlamaEngine: invalid LLAMA_SERVER_PORT '{}', "
                             "using default {}", env_port, port);
            }
        }

        spdlog::info("LlamaEngine: config for model '{}', server at {}:{}",
                     cfg.model_name, host, port);

        // Probe the server with a health check
        probe_server();
    }

    ~Impl() = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    /// Probe llama-server's /health endpoint.
    void probe_server() {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(3);
        cli.set_read_timeout(5);

        auto res = cli.Get("/health");
        if (res && res->status == 200) {
            server_running = true;
            spdlog::info("LlamaEngine: llama-server is reachable at {}:{}",
                         host, port);
        } else {
            server_running = false;
            spdlog::warn("LlamaEngine: llama-server not reachable at {}:{}",
                         host, port);
        }
    }

    /// POST JSON to a llama-server endpoint and return parsed JSON.
    auto make_request(const std::string& endpoint,
                      const nlohmann::json& body)
        -> std::expected<nlohmann::json, std::string> {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5);
        cli.set_read_timeout(120);   // 2 minutes for generation

        auto res = cli.Post(endpoint, body.dump(), "application/json");

        if (!res) {
            server_running = false;
            return std::unexpected(
                std::format("llama-server not reachable at {}:{}", host, port));
        }

        if (res->status != 200) {
            return std::unexpected(
                std::format("llama-server returned HTTP {}: {}",
                            res->status, res->body));
        }

        try {
            return nlohmann::json::parse(res->body);
        } catch (const nlohmann::json::parse_error& e) {
            return std::unexpected(
                std::format("Failed to parse llama-server response: {}",
                            e.what()));
        }
    }
};

// ---------------------------------------------------------------------------
// constructors / assignment / destructor
// ---------------------------------------------------------------------------
LlamaEngine::LlamaEngine(const LocalModelConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

LlamaEngine::~LlamaEngine() = default;

LlamaEngine::LlamaEngine(LlamaEngine&&) noexcept = default;
LlamaEngine& LlamaEngine::operator=(LlamaEngine&&) noexcept = default;

// ---------------------------------------------------------------------------
// generate — POST /v1/chat/completions (OpenAI-compatible)
// ---------------------------------------------------------------------------
auto LlamaEngine::generate(std::string_view prompt,
                            uint32_t max_tokens)
    -> std::expected<InferenceResult, std::string> {
    nlohmann::json body = {
        {"model",       impl_->config.model_name},
        {"messages",    nlohmann::json::array({
            {{"role", "user"}, {"content", std::string(prompt)}}
        })},
        {"max_tokens",  max_tokens},
        {"temperature", impl_->config.temperature},
        {"top_p",       impl_->config.top_p},
        {"stream",      false},
    };

    auto start = std::chrono::steady_clock::now();
    auto result = impl_->make_request("/v1/chat/completions", body);
    auto end   = std::chrono::steady_clock::now();

    if (!result) {
        return std::unexpected(result.error());
    }

    const auto& j = *result;

    InferenceResult ir;
    ir.engine_name = "llama.cpp";
    ir.model_name  = j.value("model", impl_->config.model_name);

    // Extract response text from choices[0].message.content
    if (j.contains("choices") && j["choices"].is_array() &&
        !j["choices"].empty()) {
        const auto& choice = j["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content")) {
            ir.text = choice["message"]["content"].get<std::string>();
        }
    }

    // Extract token usage
    if (j.contains("usage") && j["usage"].contains("completion_tokens")) {
        ir.tokens_generated = j["usage"]["completion_tokens"].get<uint32_t>();
    } else {
        ir.tokens_generated = 0;
    }

    // Calculate tokens_per_second from wall-clock timing
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          end - start).count();
    if (elapsed_ms > 0 && ir.tokens_generated > 0) {
        ir.tokens_per_second =
            static_cast<float>(ir.tokens_generated) /
            (static_cast<float>(elapsed_ms) / 1000.0f);
    } else {
        ir.tokens_per_second = 0.0f;
    }

    impl_->server_running = true;
    return ir;
}

// ---------------------------------------------------------------------------
// episodic_generate
// ---------------------------------------------------------------------------
auto LlamaEngine::episodic_generate(std::generator<euxis::runtime::SessionMessage> episodes,
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
// supports_model — GET /v1/models, fall back to config match
// ---------------------------------------------------------------------------
auto LlamaEngine::supports_model(std::string_view name) -> bool {
    httplib::Client cli(impl_->host, impl_->port);
    cli.set_connection_timeout(3);
    cli.set_read_timeout(5);

    auto res = cli.Get("/v1/models");
    if (res && res->status == 200) {
        try {
            auto j = nlohmann::json::parse(res->body);
            if (j.contains("data") && j["data"].is_array()) {
                for (const auto& model : j["data"]) {
                    auto id = model.value("id", "");
                    if (id == name) {
                        return true;
                    }
                }
            }
            // Model not found in server's list
            return false;
        } catch (const std::exception&) {
            // Parse error — fall through to config match
        }
    }

    // Server unreachable or parse error: fall back to matching config
    return impl_->config.model_name == name;
}

// ---------------------------------------------------------------------------
// health — GET /health from llama-server
// ---------------------------------------------------------------------------
auto LlamaEngine::health() -> nlohmann::json {
    httplib::Client cli(impl_->host, impl_->port);
    cli.set_connection_timeout(3);
    cli.set_read_timeout(5);

    nlohmann::json status;
    status["engine"]       = "llama.cpp";
    status["model"]        = impl_->config.model_name;
    status["context_size"] = impl_->config.context_size;
    status["host"]         = impl_->host;
    status["port"]         = impl_->port;

    auto res = cli.Get("/health");

    if (!res) {
        impl_->server_running = false;
        status["status"] = "unreachable";
        status["error"]  = httplib::to_string(res.error());
        return status;
    }

    if (res->status != 200) {
        status["status"]    = "error";
        status["http_code"] = res->status;
        return status;
    }

    impl_->server_running = true;
    status["status"] = "ok";

    // llama-server /health returns JSON with a "status" field
    try {
        auto body = nlohmann::json::parse(res->body);
        status["server_status"] = body.value("status", "unknown");
    } catch (const std::exception&) {
        // Non-JSON health response is fine — server is up
    }

    return status;
}

} // namespace euxis::inference
