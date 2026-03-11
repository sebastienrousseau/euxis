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
    Message msg;
    // We expect a timeout since no server is listening
    auto result = transport.send(msg);
    EXPECT_FALSE(result.has_value());
}

// Internal logic coverage via dummy data
// (In a more complex setup we'd use a MockWebSocket, but here we can 
// verify no-crash/branch coverage for the parsing logic if exposed)
// For this audit, we focus on the public send() timeout and construction.

} // namespace
} // namespace euxis::a2a
