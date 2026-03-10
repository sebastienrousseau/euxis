#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

/** @namespace ix
 * @brief Forward declaration for IXWebSocket library.
 */
namespace ix {
class WebSocket;
} // namespace ix

namespace euxis::core {

/** @brief Type for WebSocket response handlers. */
using WsResponseCallback = std::function<void(const nlohmann::json&)>;

/**
 * @brief High-level WebSocket client for agent-gateway communication.
 * 
 * This class wraps the IXWebSocket implementation to provide a clean, 
 * synchronous (send-and-wait) and asynchronous interface for agent messaging.
 */
class WebSocketClient {
public:
    /**
     * @brief Construct a new WebSocket Client.
     * @param url The full ws:// or wss:// URL to connect to.
     */
    explicit WebSocketClient(const std::string& url);
    
    /** @brief Ensure connection is stopped on destruction. */
    ~WebSocketClient();

    // Move-only semantics for safe resource management
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;
    WebSocketClient(WebSocketClient&&) noexcept = default;
    WebSocketClient& operator=(WebSocketClient&&) noexcept = default;

    /** @brief Initiates the background connection and waits for handshake. */
    void connect();
    
    /** @brief Stops the connection. */
    void disconnect();

    /** @brief Returns true if the WebSocket is currently open. */
    [[nodiscard]] auto is_connected() const -> bool;

    /**
     * @brief Send a JSON message and block until a response is received.
     * 
     * @param message The request to send.
     * @param timeout Maximum time to wait for a reply.
     * @return std::optional<nlohmann::json> The response, or nullopt if timeout.
     */
    auto send_and_wait(const nlohmann::json& message,
                       std::chrono::milliseconds timeout =
                           std::chrono::milliseconds(5000))
        -> std::optional<nlohmann::json>;

    /**
     * @brief Send a JSON message without waiting for a response.
     * @param message The message to dispatch.
     */
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
