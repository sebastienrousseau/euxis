#include <euxis/network/ws_client.hpp>

#include <cassert>

#include <spdlog/spdlog.h>

namespace euxis::network {

WebSocketClient::WebSocketClient(const std::string& url) : url_(url) {
    assert(!url.empty() && "P10-R5: WebSocket URL must not be empty");
    ws_.setUrl(url);
    ws_.setPingInterval(30);
    ws_.enablePong();
    
    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            spdlog::debug("WebSocket message received from {}: {}", url_, msg->str);
            auto parsed = nlohmann::json::parse(msg->str, nullptr, false);
            if (!parsed.is_discarded()) {
                std::lock_guard<std::mutex> lock(mutex_);
                last_response_ = std::move(parsed);
                cv_.notify_all();
            }
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

    assert(timeout_secs > 0 && "P10-R5: timeout must be positive");
    assert(!message.empty() && "P10-R5: message must not be empty");

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
    // cv_.wait_for predicate guarantees has_value() == true here; the
    // explicit re-check makes it visible to clang-tidy's dataflow without
    // changing behaviour. The unexpected branch is unreachable barring a
    // logic bug in the wait predicate.
    if (!last_response_.has_value()) {
        return std::unexpected("WebSocket response missing after signal");
    }
    return last_response_.value();
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
