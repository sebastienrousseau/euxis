#include <gtest/gtest.h>
#include "euxis/network/ws_client.hpp"

namespace euxis::network {
namespace {

TEST(WebSocketClientTest, BasicConstruction) {
    EXPECT_NO_THROW({
        WebSocketClient client("ws://localhost:9999");
    });
}

TEST(WebSocketClientTest, MethodsNoThrow) {
    WebSocketClient client("ws://localhost:9999");
    EXPECT_NO_THROW(client.connect());
    EXPECT_NO_THROW(client.send("test"));
}

TEST(WebSocketClientTest, SetOnMessage) {
    WebSocketClient client("ws://localhost:9999");
    EXPECT_NO_THROW({
        client.set_on_message([](const std::string& msg) { (void)msg; });
    });
}

} // namespace
} // namespace euxis::network
