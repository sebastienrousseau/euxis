#include <euxis/network/ws_client.hpp>
#include <spdlog/spdlog.h>

namespace euxis::network {

WebSocketClient::WebSocketClient(const std::string& url) : url_(url) {
    ws_.setUrl(url);
    
    // Resilience: Set connection timeout and heartbeats
    ix::WebSocketOptions options;
    options.connectTimeout = 10; // 10 seconds
    options.pingInterval = 30;    // 30 seconds keepalive
    ws_.setOptions(options);

    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            spdlog::debug("WebSocket message received from {}: {}", url_, msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            spdlog::error("WebSocket error from {}: {}", url_, msg->errorInfo.reason);
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            spdlog::warn("WebSocket connection closed for {}: {}", url_, msg->closeInfo.reason);
        }
    });
}

WebSocketClient::~WebSocketClient() {
    ws_.stop();
}

void WebSocketClient::connect() {
    ws_.start();
}

void WebSocketClient::send(const std::string& message) {
    ws_.send(message);
}

void WebSocketClient::set_on_message(std::function<void(const std::string&)> on_message) {
    ws_.setOnMessageCallback([on_message](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            on_message(msg->str);
        }
    });
}

} // namespace euxis::network
