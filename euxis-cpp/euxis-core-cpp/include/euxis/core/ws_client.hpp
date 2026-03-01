#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace ix {
class WebSocket;
} // namespace ix

namespace euxis::core {

using WsResponseCallback = std::function<void(const nlohmann::json&)>;

class WebSocketClient {
public:
    explicit WebSocketClient(const std::string& url);
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    void connect();
    void disconnect();

    [[nodiscard]] auto is_connected() const -> bool;

    /// Send a JSON message and wait for a response (synchronous).
    auto send_and_wait(const nlohmann::json& message,
                       std::chrono::milliseconds timeout =
                           std::chrono::milliseconds(5000))
        -> std::optional<nlohmann::json>;

    /// Send a JSON message (fire-and-forget).
    void send(const nlohmann::json& message);

private:
    std::string url_;
    std::unique_ptr<ix::WebSocket> ws_;
    bool connected_{false};

    std::mutex response_mutex_;
    std::condition_variable response_cv_;
    std::optional<nlohmann::json> last_response_;
};

} // namespace euxis::core
