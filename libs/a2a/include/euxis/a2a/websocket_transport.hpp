#pragma once

#include <euxis/a2a/transport.hpp>
#include <euxis/network/ws_client.hpp>
#include <condition_variable>
#include <mutex>
#include <map>

namespace euxis::a2a {

/**
 * @brief High-performance asynchronous WebSocket transport for A2A communication.
 */
class WebSocketA2ATransport : public ITransport {
public:
    explicit WebSocketA2ATransport(const std::string& url);
    ~WebSocketA2ATransport() override = default;

    auto send(const A2AMessage& msg) -> std::expected<A2AMessage, TransportError> override;

private:
    euxis::network::WebSocketClient client_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::map<std::string, std::expected<A2AMessage, TransportError>> pending_responses_;
};

} // namespace euxis::a2a
