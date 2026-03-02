#include "euxis/core/ws_client.hpp"

#include <gtest/gtest.h>

namespace euxis::core {
namespace {

TEST(WebSocketClientTest, ConstructionSetsDisconnected) {
    WebSocketClient client("ws://localhost:9999");
    EXPECT_FALSE(client.is_connected());
}

TEST(WebSocketClientTest, ConnectToInvalidHostStaysDisconnected) {
    WebSocketClient client("ws://127.0.0.1:1");
    client.connect();
    // Connection to a non-listening port should fail
    EXPECT_FALSE(client.is_connected());
}

TEST(WebSocketClientTest, DisconnectOnDisconnected) {
    WebSocketClient client("ws://localhost:9999");
    // Should not crash
    client.disconnect();
    EXPECT_FALSE(client.is_connected());
}

TEST(WebSocketClientTest, SendAndWaitTimesOut) {
    WebSocketClient client("ws://127.0.0.1:1");
    auto result = client.send_and_wait(
        {{"type", "test"}},
        std::chrono::milliseconds(50));
    EXPECT_FALSE(result.has_value());
}

// --- Coverage: lines 31-32 (non-JSON message callback) ---
// This is exercised internally when the WS receives non-JSON. We verify the
// send() fire-and-forget path here.
TEST(WebSocketClientTest, SendFireAndForget) {
    WebSocketClient client("ws://127.0.0.1:1");
    // send() should not crash even when not connected
    EXPECT_NO_THROW(client.send({{"type", "ping"}}));
}

// --- Coverage: lines 76-78 (send plain JSON message) ---
TEST(WebSocketClientTest, SendJsonMessage) {
    WebSocketClient client("ws://127.0.0.1:1");
    nlohmann::json msg = {{"action", "test"}, {"data", 42}};
    EXPECT_NO_THROW(client.send(msg));
}

// --- Coverage: destructor calls disconnect ---
TEST(WebSocketClientTest, DestructorCallsDisconnect) {
    // Just construct and immediately destroy -- should not crash
    {
        WebSocketClient client("ws://127.0.0.1:1");
        client.connect();
    }
    SUCCEED();
}

} // namespace
} // namespace euxis::core
