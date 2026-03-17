#include "euxis/gateway/websocket.hpp"

#include <ixwebsocket/IXWebSocketServer.h>
#include <spdlog/spdlog.h>

namespace euxis::gateway {

WebSocketHub::WebSocketHub(int port, const std::string& host)
    : port_(port), host_(host) {}

WebSocketHub::~WebSocketHub() { stop(); }

void WebSocketHub::set_message_handler(WsMessageHandler handler) {
    handler_ = std::move(handler);
}

void WebSocketHub::start() {
    server_ = std::make_unique<ix::WebSocketServer>(port_, host_);

    server_->setOnConnectionCallback(
        [this](std::weak_ptr<ix::WebSocket> ws_weak,
               std::shared_ptr<ix::ConnectionState> state) {
            auto client_id = state->getId();
            
            auto ws = ws_weak.lock();
            if (!ws) return;

            {
                std::lock_guard lock(clients_mutex_);
                clients_[client_id] = ws;
            }
            spdlog::info("WS client connected: {}", client_id);

            ws->setOnMessageCallback(
                [this, client_id, ws_weak](const ix::WebSocketMessagePtr& msg) {
                    if (msg->type == ix::WebSocketMessageType::Close) {
                        spdlog::info("WS client disconnected: {}", client_id);
                        std::lock_guard lock(clients_mutex_);
                        clients_.erase(client_id);
                    } else if (msg->type == ix::WebSocketMessageType::Message) {
                        if (!handler_) return;
                        auto ws_ptr = ws_weak.lock();
                        if (!ws_ptr) return;
                        try {
                            auto request = nlohmann::json::parse(msg->str);
                            auto response = handler_(client_id, request);
                            ws_ptr->send(response.dump());
                        } catch (const nlohmann::json::parse_error&) {
                            nlohmann::json err = {
                                {"error", "invalid_json"},
                                {"message", "Failed to parse message"},
                            };
                            ws_ptr->send(err.dump());
                        }
                    }
                });
        });

    auto res = server_->listen();
    if (!res.first) {
        spdlog::error("Failed to start WebSocket server on {}:{}: {}",
                       host_, port_, res.second);
        return;
    }
    server_->start();
    spdlog::info("WebSocket server started on {}:{}", host_, port_);
}

void WebSocketHub::stop() {
    if (server_) {
        server_->stop();
        server_.reset();
    }
    std::lock_guard lock(clients_mutex_);
    clients_.clear();
}

void WebSocketHub::broadcast(const nlohmann::json& message) {
    auto payload = message.dump();
    std::lock_guard lock(clients_mutex_);
    for (auto& [id, ws] : clients_) {
        if (ws) ws->send(payload);
    }
}

void WebSocketHub::send_to(const std::string& client_id,
                            const nlohmann::json& message) {
    std::lock_guard lock(clients_mutex_);
    auto it = clients_.find(client_id);
    if (it != clients_.end() && it->second) {
        it->second->send(message.dump());
    }
}

auto WebSocketHub::client_count() const -> size_t {
    std::lock_guard lock(clients_mutex_);
    return clients_.size();
}

} // namespace euxis::gateway
