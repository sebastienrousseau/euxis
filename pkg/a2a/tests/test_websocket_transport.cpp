#include <gtest/gtest.h>
#include "euxis/a2a/websocket_transport.hpp"

namespace euxis::a2a {
namespace {

TEST(WebSocketA2ATransportTest, BasicConstruction) {
    EXPECT_NO_THROW({
        WebSocketA2ATransport transport("ws://localhost:9999");
    });
}

TEST(WebSocketA2ATransportTest, SendTimesOutWhenNoServer) {
    WebSocketA2ATransport transport("ws://127.0.0.1:1");
    Message msg;
    // This will hit the 10s timeout since no server responds
    // For unit tests, we might want a shorter timeout or a mock, 
    // but this verifies the timeout logic path.
    auto result = transport.send(msg);
    EXPECT_FALSE(result.has_value());
    if (!result.has_value()) {
        EXPECT_EQ(result.error(), TransportError::Timeout);
    }
}

} // namespace
} // namespace euxis::a2a
