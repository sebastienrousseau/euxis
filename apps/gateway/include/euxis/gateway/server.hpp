#pragma once

#include <memory>
#include <string>

// httplib.h triggers GCC's -Wmaybe-uninitialized; Clang has no such
// flag and -Wunknown-warning-option is -Werror under AppleClang.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <httplib.h>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

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
