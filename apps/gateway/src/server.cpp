#include "euxis/gateway/server.hpp"
#include "euxis/gateway/routes.hpp"

#include <spdlog/spdlog.h>
#include <sodium.h>

namespace euxis::gateway {

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
    register_health_routes(server_);
    register_session_routes(server_);
    register_webhook_routes(server_);
    register_admin_routes(server_);
    register_mcp_routes(server_);
}

void GatewayServer::setup_ws_handlers() {
    ws_hub_.set_message_handler(
        [](const std::string& client_id,
           const nlohmann::json& message) -> nlohmann::json {
            auto type = message.value("type", "");

            if (type == "ping") {
                return {{"type", "pong"},
                        {"client_id", client_id}};
            }

            if (type == "dispatch") {
                auto agent = message.value("agent", "");
                auto task = message.value("task", "");
                spdlog::info("WS dispatch: agent={}, task={}", agent, task);
                return {{"type", "dispatch_ack"},
                        {"agent", agent},
                        {"status", "accepted"}};
            }

            if (type == "subscribe") {
                auto channel = message.value("channel", "");
                return {{"type", "subscribed"},
                        {"channel", channel}};
            }

            return {{"type", "error"},
                    {"message", "Unknown message type: " + type}};
        });
}

} // namespace euxis::gateway
