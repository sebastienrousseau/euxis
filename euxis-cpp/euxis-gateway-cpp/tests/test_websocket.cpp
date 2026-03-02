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

TEST_F(WebSocketHubTest, BroadcastToNoClients) {
    WebSocketHub hub(TEST_PORT + 4);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json&) -> nlohmann::json {
            return {{"ok", true}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Broadcast with no clients should not crash
    hub.broadcast({{"type", "announcement"}, {"data", "hello"}});
    hub.stop();
}

TEST_F(WebSocketHubTest, SendToNonExistentClient) {
    WebSocketHub hub(TEST_PORT + 5);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json&) -> nlohmann::json {
            return {{"ok", true}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send to a client ID that doesn't exist should not crash
    hub.send_to("nonexistent-client-id", {{"type", "message"}});
    hub.stop();
}

TEST_F(WebSocketHubTest, MultipleMessagesInSequence) {
    WebSocketHub hub(TEST_PORT + 6);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json& msg) -> nlohmann::json {
            return {{"echo", msg}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    euxis::core::WebSocketClient client(
        "ws://127.0.0.1:" + std::to_string(TEST_PORT + 6));
    client.connect();
    ASSERT_TRUE(client.is_connected());

    // Send multiple messages in sequence
    auto resp1 = client.send_and_wait(
        {{"type", "msg1"}}, std::chrono::milliseconds(2000));
    ASSERT_TRUE(resp1.has_value());

    auto resp2 = client.send_and_wait(
        {{"type", "msg2"}}, std::chrono::milliseconds(2000));
    ASSERT_TRUE(resp2.has_value());

    client.disconnect();
    hub.stop();
}

TEST_F(WebSocketHubTest, StopWithoutStart) {
    WebSocketHub hub(TEST_PORT + 7);
    // Stopping without starting should not crash
    hub.stop();
}

TEST_F(WebSocketHubTest, DoubleStop) {
    WebSocketHub hub(TEST_PORT + 8);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json&) -> nlohmann::json {
            return {{"ok", true}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    hub.stop();
    // Second stop should not crash
    hub.stop();
}

TEST_F(WebSocketHubTest, BroadcastWithConnectedClient) {
    WebSocketHub hub(TEST_PORT + 9);
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json& msg) -> nlohmann::json {
            return {{"echo", msg}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    euxis::core::WebSocketClient client(
        "ws://127.0.0.1:" + std::to_string(TEST_PORT + 9));
    client.connect();
    ASSERT_TRUE(client.is_connected());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Broadcast should reach the connected client
    hub.broadcast({{"type", "broadcast"}, {"data", "hello all"}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    client.disconnect();
    hub.stop();
}

TEST_F(WebSocketHubTest, NoMessageHandler) {
    WebSocketHub hub(TEST_PORT + 10);
    // Do NOT set a message handler
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    euxis::core::WebSocketClient client(
        "ws://127.0.0.1:" + std::to_string(TEST_PORT + 10));
    client.connect();
    ASSERT_TRUE(client.is_connected());

    // Send a message without handler - should not crash
    // Use fire-and-forget since there won't be a response without handler
    client.send({{"type", "test"}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    client.disconnect();
    hub.stop();
}

TEST_F(WebSocketHubTest, CustomHost) {
    // Construct with custom host
    WebSocketHub hub(TEST_PORT + 11, "127.0.0.1");
    hub.set_message_handler(
        [](const std::string&, const nlohmann::json&) -> nlohmann::json {
            return {{"ok", true}};
        });
    hub.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(hub.client_count(), 0);
    hub.stop();
}

} // namespace
} // namespace euxis::gateway
