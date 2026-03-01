#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace ix {
class WebSocketServer;
class WebSocket;
} // namespace ix

namespace euxis::gateway {

using WsMessageHandler =
    std::function<nlohmann::json(const std::string& client_id,
                                  const nlohmann::json& message)>;

class WebSocketHub {
public:
    explicit WebSocketHub(int port = 8081,
                          const std::string& host = "0.0.0.0");
    ~WebSocketHub();

    WebSocketHub(const WebSocketHub&) = delete;
    WebSocketHub& operator=(const WebSocketHub&) = delete;

    void set_message_handler(WsMessageHandler handler);
    void start();
    void stop();

    void broadcast(const nlohmann::json& message);
    void send_to(const std::string& client_id, const nlohmann::json& message);

    [[nodiscard]] auto port() const -> int { return port_; }
    [[nodiscard]] auto client_count() const -> size_t;

private:
    int port_;
    std::string host_;
    std::unique_ptr<ix::WebSocketServer> server_;
    WsMessageHandler handler_;
    mutable std::mutex clients_mutex_;
    std::unordered_map<std::string, std::shared_ptr<ix::WebSocket>> clients_;
};

} // namespace euxis::gateway
