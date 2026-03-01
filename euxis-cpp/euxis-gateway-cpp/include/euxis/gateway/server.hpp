#pragma once

#include <memory>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <httplib.h>
#pragma GCC diagnostic pop

#include "config.hpp"
#include "websocket.hpp"

namespace euxis::gateway {

class GatewayServer {
public:
    explicit GatewayServer(GatewayConfig config = {});

    void start();
    void stop();

    [[nodiscard]] auto is_running() const -> bool { return running_; }
    [[nodiscard]] auto port() const -> int { return config_.port; }
    [[nodiscard]] auto ws_port() const -> int { return config_.ws_port; }

    /// Access the underlying httplib::Server (for testing).
    auto server() -> httplib::Server& { return server_; }

    /// Access the WebSocket hub (for testing / broadcasting).
    auto ws_hub() -> WebSocketHub& { return ws_hub_; }

private:
    GatewayConfig config_;
    httplib::Server server_;
    WebSocketHub ws_hub_;
    bool running_{false};

    void register_routes();
    void setup_ws_handlers();
};

} // namespace euxis::gateway
