#pragma once

#include <ixwebsocket/IXWebSocket.h>
#include <memory>
#include <string>

namespace euxis::network {

class WebSocketClient {
public:
    explicit WebSocketClient(const std::string& url);
    ~WebSocketClient();

    void connect();
    void send(const std::string& message);
    void set_on_message(std::function<void(const std::string&)> on_message);

private:
    ix::WebSocket ws_;
    std::string url_;
};

} // namespace euxis::network
