#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "euxis/gateway/websocket.hpp"
#include "euxis/core/ws_client.hpp"

namespace euxis::gateway {
namespace {

class WebSocketHubTest : public ::testing::Test {
protected:
    static constexpr int TEST_PORT = 19876;
};

TEST_F(WebSocketHubTest, StartAndStop) {
    WebSocketHub hub(TEST_PORT);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json& msg) -> nlohmann::json {
            return {{"echo", msg}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(hub.client_count(), 0);
    hub.stop();
}

TEST_F(WebSocketHubTest, ClientConnectAndPing) {
    WebSocketHub hub(TEST_PORT + 1);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json& msg) -> nlohmann::json {
            auto type = msg.value("type", "");
            if (type == "ping") {
                return {{"type", "pong"}};
            }
            return {{"type", "echo"}, {"data", msg}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Connect client
    euxis::core::WebSocketClient client(
        "ws://127.0.0.1:" + std::to_string(TEST_PORT + 1));
    client.connect();
    ASSERT_TRUE(client.is_connected());

    // Send ping and wait for pong
    auto response = client.send_and_wait(
        {{"type", "ping"}},
        std::chrono::milliseconds(2000));
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "pong");

    client.disconnect();
    hub.stop();
}

TEST_F(WebSocketHubTest, ClientCountUpdates) {
    WebSocketHub hub(TEST_PORT + 2);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json&) -> nlohmann::json {
            return {{"ok", true}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    euxis::core::WebSocketClient client(
        "ws://127.0.0.1:" + std::to_string(TEST_PORT + 2));
    client.connect();
    ASSERT_TRUE(client.is_connected());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(hub.client_count(), 1);

    client.disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    hub.stop();
}

TEST_F(WebSocketHubTest, DispatchMessage) {
    WebSocketHub hub(TEST_PORT + 3);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json& msg) -> nlohmann::json {
            if (msg.value("type", "") == "dispatch") {
                return {{"type", "dispatch_ack"},
                        {"agent", msg.value("agent", "")},
                        {"status", "accepted"}};
            }
            return {{"type", "error"}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    euxis::core::WebSocketClient client(
        "ws://127.0.0.1:" + std::to_string(TEST_PORT + 3));
    client.connect();
    ASSERT_TRUE(client.is_connected());

    auto response = client.send_and_wait(
        {{"type", "dispatch"},
         {"agent", "architect"},
         {"task", "Design system"}},
        std::chrono::milliseconds(2000));
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "dispatch_ack");
    EXPECT_EQ((*response)["agent"], "architect");
    EXPECT_EQ((*response)["status"], "accepted");

    client.disconnect();
    hub.stop();
}

} // namespace
} // namespace euxis::gateway
