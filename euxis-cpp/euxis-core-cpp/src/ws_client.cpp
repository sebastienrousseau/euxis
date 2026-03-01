#include "euxis/core/ws_client.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <spdlog/spdlog.h>

namespace euxis::core {

WebSocketClient::WebSocketClient(const std::string& url)
    : url_(url), ws_(std::make_unique<ix::WebSocket>()) {
    ws_->setUrl(url_);
}

WebSocketClient::~WebSocketClient() { disconnect(); }

void WebSocketClient::connect() {
    ws_->setOnMessageCallback(
        [this](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                connected_ = true;
                spdlog::debug("WS connected to {}", url_);
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                connected_ = false;
                spdlog::debug("WS disconnected from {}", url_);
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                try {
                    auto response = nlohmann::json::parse(msg->str);
                    std::lock_guard lock(response_mutex_);
                    last_response_ = std::move(response);
                    response_cv_.notify_one();
                } catch (...) {
                    spdlog::warn("WS received non-JSON message");
                }
            } else if (msg->type == ix::WebSocketMessageType::Error) {
                spdlog::error("WS error: {}", msg->errorInfo.reason);
            }
        });

    ws_->start();

    // Wait briefly for connection
    for (int i = 0; i < 50 && !connected_; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void WebSocketClient::disconnect() {
    if (ws_) {
        ws_->stop();
        connected_ = false;
    }
}

auto WebSocketClient::is_connected() const -> bool {
    return connected_;
}

auto WebSocketClient::send_and_wait(
    const nlohmann::json& message,
    std::chrono::milliseconds timeout) -> std::optional<nlohmann::json> {
    {
        std::lock_guard lock(response_mutex_);
        last_response_.reset();
    }

    ws_->send(message.dump());

    std::unique_lock lock(response_mutex_);
    if (response_cv_.wait_for(lock, timeout, [this] {
            return last_response_.has_value();
        })) {
        return std::move(last_response_);
    }
    return std::nullopt;
}

void WebSocketClient::send(const nlohmann::json& message) {
    ws_->send(message.dump());
}

} // namespace euxis::core
