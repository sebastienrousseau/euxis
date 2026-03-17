#include <euxis/network/ws_client.hpp>
#include <spdlog/spdlog.h>

namespace euxis::network {

WebSocketClient::WebSocketClient(const std::string& url) : url_(url) {
    ws_.setUrl(url);
    ws_.setPingInterval(30);
    ws_.enablePong();
    
    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            spdlog::debug("WebSocket message received from {}: {}", url_, msg->str);
            try {
                std::lock_guard<std::mutex> lock(mutex_);
                last_response_ = nlohmann::json::parse(msg->str);
                cv_.notify_all();
            } catch (const std::exception&) {}
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

void WebSocketClient::disconnect() {
    ws_.stop();
}

void WebSocketClient::send(const std::string& message) {
    ws_.send(message);
}

void WebSocketClient::send(const nlohmann::json& message) {
    ws_.send(message.dump());
}

auto WebSocketClient::send_and_wait(const nlohmann::json& message, int timeout_secs) 
    -> std::expected<nlohmann::json, std::string> {
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_response_ = std::nullopt;
    }

    send(message);

    std::unique_lock<std::mutex> lock(mutex_);
    bool signaled = cv_.wait_for(lock, std::chrono::seconds(timeout_secs), [this] {
        return last_response_.has_value();
    });

    if (!signaled) return std::unexpected("WebSocket request timed out");
    return *last_response_;
}

bool WebSocketClient::is_connected() const {
    return ws_.getReadyState() == ix::ReadyState::Open;
}

void WebSocketClient::set_on_message(std::function<void(const std::string&)> on_message) {
    ws_.setOnMessageCallback([on_message](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            on_message(msg->str);
        }
    });
}

} // namespace euxis::network
