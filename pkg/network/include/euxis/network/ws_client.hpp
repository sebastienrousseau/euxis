#pragma once

#include <ixwebsocket/IXWebSocket.h>
#include <expected>
#include <memory>
#include <string>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>

namespace euxis::network {

class WebSocketClient {
public:
    explicit WebSocketClient(const std::string& url);
    ~WebSocketClient();

    void connect();
    void disconnect();
    void send(const std::string& message);
    void send(const nlohmann::json& message);

    [[nodiscard]] auto send_and_wait(const nlohmann::json& message, int timeout_secs = 10) 
        -> std::expected<nlohmann::json, std::string>;

    [[nodiscard]] bool is_connected() const;

    void set_on_message(std::function<void(const std::string&)> on_message);

private:
    ix::WebSocket ws_;
    std::string url_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<nlohmann::json> last_response_;
};

} // namespace euxis::network
