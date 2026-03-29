#include "euxis/gateway/server.hpp"
#include "euxis/gateway/auth.hpp"
#include "euxis/gateway/routes.hpp"

#include <spdlog/spdlog.h>
#include <sodium.h>

namespace euxis::gateway {

bool authorize_request(const httplib::Request& req, httplib::Response& res,
                       const RouteContext& ctx) {
    if (req.body.size() > kMaxRequestBodySize) {
        res.status = 413;
        res.set_content(R"({"error":"request body too large"})", "application/json");
        return false;
    }
    auto token = extract_bearer(req);
    auto result = verify_bearer_token(token, ctx.auth_token);
    if (!result.has_value()) {
        res.status = result.error().status_code;
        nlohmann::json err_body = {{"error", result.error().message}};
        res.set_content(err_body.dump(), "application/json");
        return false;
    }
    return true;
}

GatewayServer::GatewayServer(GatewayConfig config)
    : config_(std::move(config)),
      ws_hub_(config_.ws_port, config_.host) {
    if (sodium_init() < 0) {
        spdlog::error("Failed to initialize libsodium");
    }
    register_routes();
    setup_ws_handlers();
}

void GatewayServer::start() {
    spdlog::info("Starting Euxis Gateway on {}:{} (WS: {})",
                 config_.host, config_.port, config_.ws_port);
    ws_hub_.start();
    running_ = true;
    server_.listen(config_.host, config_.port);
    running_ = false;
}

void GatewayServer::stop() {
    server_.stop();
    ws_hub_.stop();
    running_ = false;
}

void GatewayServer::register_routes() {
    // Extract auth token from config
    RouteContext ctx;
    if (config_.raw.contains("gateway") && config_.raw["gateway"].contains("auth") &&
        config_.raw["gateway"]["auth"].contains("token")) {
        ctx.auth_token = config_.raw["gateway"]["auth"]["token"].value("value", "");
    }

    register_health_routes(server_);
    register_session_routes(server_, ctx);
    register_webhook_routes(server_, ctx);
    register_admin_routes(server_, ctx);
    register_mcp_routes(server_, ctx);
}

void GatewayServer::setup_ws_handlers() {
    // Capture auth token for WebSocket authentication.
    std::string ws_auth_token;
    if (config_.raw.contains("gateway") && config_.raw["gateway"].contains("auth") &&
        config_.raw["gateway"]["auth"].contains("token")) {
        ws_auth_token = config_.raw["gateway"]["auth"]["token"].value("value", "");
    }

    ws_hub_.set_message_handler(
        [ws_auth_token](const std::string& client_id,
           const nlohmann::json& message) -> nlohmann::json {
            // Helper: lookup string field without temporary allocation.
            static const std::string empty_str;
            auto get_str = [&](const char* key) -> const std::string& {
                auto it = message.find(key);
                if (it == message.end() || !it->is_string())
                    return empty_str;
                return it->get_ref<const std::string&>();
            };

            const auto& type = get_str("type");

            // Ping is the most frequent control message — skip auth.
            if (type == "ping") {
                return {{"type", "pong"},
                        {"client_id", client_id}};
            }

            // Authenticate before processing commands.
            if (!ws_auth_token.empty()) {
                const auto& token = get_str("token");
                if (token.size() != ws_auth_token.size() ||
                    sodium_memcmp(token.data(), ws_auth_token.data(),
                                  ws_auth_token.size()) != 0) {
                    return {{"type", "error"},
                            {"message", "Authentication required"}};
                }
            }

            if (type == "dispatch") {
                const auto& agent = get_str("agent");
                const auto& task = get_str("task");
                spdlog::info("WS dispatch: agent={}, task={}", agent, task);
                return {{"type", "dispatch_ack"},
                        {"agent", agent},
                        {"status", "accepted"}};
            }

            if (type == "subscribe") {
                const auto& channel = get_str("channel");
                return {{"type", "subscribed"},
                        {"channel", channel}};
            }

            return {{"type", "error"},
                    {"message", "Unknown message type"}};
        });
}

} // namespace euxis::gateway
