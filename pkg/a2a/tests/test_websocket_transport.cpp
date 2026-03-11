#include <gtest/gtest.h>
#include "euxis/a2a/websocket_transport.hpp"
#include <nlohmann/json.hpp>

namespace euxis::a2a {
namespace {

TEST(WebSocketA2ATransportTest, BasicConstruction) {
    EXPECT_NO_THROW({
        WebSocketA2ATransport transport("ws://localhost:9999");
    });
}

TEST(WebSocketA2ATransportTest, SendTimesOut) {
    WebSocketA2ATransport transport("ws://127.0.0.1:1");
    A2AMessage msg;
    auto result = transport.send(msg);
    EXPECT_FALSE(result.has_value());
}

} // namespace
} // namespace euxis::a2a
