#include <gtest/gtest.h>
#include <sodium.h>

#include "euxis/gateway/mcp.hpp"

namespace euxis::gateway {
namespace {

class McpHostTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0);
    }

    McpHost host;

    static auto make_request(const std::string& method,
                             const nlohmann::json& params = {},
                             const nlohmann::json& id = 1) -> nlohmann::json {
        nlohmann::json req = {{"jsonrpc", "2.0"}, {"method", method}, {"id", id}};
        if (!params.is_null()) {
            req["params"] = params;
        }
        return req;
    }

    void initialize() {
        host.handle_request(make_request("initialize"));
    }
};

TEST_F(McpHostTest, InitializationRequired) {
    auto res = host.handle_request(make_request("tools/list"));
    EXPECT_EQ(res["error"]["code"], -32002);
    EXPECT_EQ(res["error"]["message"], "Server not initialized");
}

TEST_F(McpHostTest, InitializeReturnsCapabilities) {
    auto res = host.handle_request(make_request("initialize"));
    ASSERT_TRUE(res.contains("result"));
    auto& result = res["result"];
    EXPECT_EQ(result["protocolVersion"], "2024-11-05");
    EXPECT_EQ(result["serverInfo"]["name"], "euxis-gateway");
    EXPECT_TRUE(result["capabilities"].contains("tools"));
}

TEST_F(McpHostTest, ToolsListReturnsRegisteredTools) {
    initialize();
    auto res = host.handle_request(make_request("tools/list"));
    auto& tools = res["result"]["tools"];
    ASSERT_GE(tools.size(), 2);

    bool has_metrics = false;
    bool has_sign = false;
    for (const auto& t : tools) {
        if (t["name"] == "get_metrics") has_metrics = true;
        if (t["name"] == "sign_payload") has_sign = true;
    }
    EXPECT_TRUE(has_metrics);
    EXPECT_TRUE(has_sign);
}

TEST_F(McpHostTest, ToolsCallGetMetrics) {
    initialize();
    nlohmann::json params = {{"name", "get_metrics"}, {"arguments", {}}};
    auto res = host.handle_request(make_request("tools/call", params));
    ASSERT_TRUE(res.contains("result"));
    auto& content = res["result"]["content"];
    ASSERT_EQ(content.size(), 1);
    EXPECT_EQ(content[0]["type"], "text");

    auto text = nlohmann::json::parse(content[0]["text"].get<std::string>());
    EXPECT_TRUE(text.contains("uptime_ms"));
    EXPECT_TRUE(text.contains("sessions_active"));
    EXPECT_TRUE(text.contains("timestamp"));
}

TEST_F(McpHostTest, ToolsCallSignPayload) {
    initialize();
    nlohmann::json params = {
        {"name", "sign_payload"},
        {"arguments", {{"payload", "test data"}}}};
    auto res = host.handle_request(make_request("tools/call", params));
    ASSERT_TRUE(res.contains("result"));
    auto& content = res["result"]["content"];
    ASSERT_EQ(content.size(), 1);

    auto text = nlohmann::json::parse(content[0]["text"].get<std::string>());
    EXPECT_EQ(text["payload"], "test data");
    EXPECT_EQ(text["algorithm"], "ed25519");
    EXPECT_FALSE(text["signature"].get<std::string>().empty());
}

TEST_F(McpHostTest, ToolsCallUnknownTool) {
    initialize();
    nlohmann::json params = {{"name", "nonexistent"}, {"arguments", {}}};
    auto res = host.handle_request(make_request("tools/call", params));
    EXPECT_EQ(res["error"]["code"], -32601);
}

TEST_F(McpHostTest, PingReturnsEmptyResult) {
    initialize();
    auto res = host.handle_request(make_request("ping"));
    ASSERT_TRUE(res.contains("result"));
    EXPECT_TRUE(res["result"].is_object());
    EXPECT_TRUE(res["result"].empty());
}

TEST_F(McpHostTest, CustomToolRegistration) {
    initialize();
    host.register_tool("echo", "Echo arguments back",
                       {{"type", "object"}},
                       [](const nlohmann::json& args) { return args; });

    nlohmann::json params = {
        {"name", "echo"},
        {"arguments", {{"msg", "hello"}}}};
    auto res = host.handle_request(make_request("tools/call", params));
    auto text = nlohmann::json::parse(
        res["result"]["content"][0]["text"].get<std::string>());
    EXPECT_EQ(text["msg"], "hello");
}

TEST_F(McpHostTest, ToolHandlerException) {
    initialize();
    host.register_tool("fail", "Always fails",
                       {{"type", "object"}},
                       [](const nlohmann::json&) -> nlohmann::json {
                           throw std::runtime_error("intentional");
                       });

    nlohmann::json params = {{"name", "fail"}, {"arguments", {}}};
    auto res = host.handle_request(make_request("tools/call", params));
    EXPECT_EQ(res["error"]["code"], -32603);
}

TEST_F(McpHostTest, ParseErrorOnInvalidRequest) {
    // Missing jsonrpc field
    nlohmann::json bad = {{"method", "ping"}};
    auto res = host.handle_request(bad);
    EXPECT_EQ(res["error"]["code"], -32600);
}

TEST_F(McpHostTest, UnknownMethod) {
    initialize();
    auto res = host.handle_request(make_request("nonexistent/method"));
    EXPECT_EQ(res["error"]["code"], -32601);
}

TEST_F(McpHostTest, NotificationsInitializedAck) {
    auto res = host.handle_request(make_request("notifications/initialized"));
    ASSERT_TRUE(res.contains("result"));
}

// --- Coverage: mcp.cpp lines 77-78 (tools() const accessor) ---
TEST_F(McpHostTest, ToolsAccessorReturnsList) {
    const auto& tools = host.tools();
    ASSERT_GE(tools.size(), 2u);
    bool has_metrics = false;
    bool has_sign = false;
    for (const auto& t : tools) {
        if (t.name == "get_metrics") has_metrics = true;
        if (t.name == "sign_payload") has_sign = true;
    }
    EXPECT_TRUE(has_metrics);
    EXPECT_TRUE(has_sign);
}

// --- Coverage: mcp.cpp line 77 (tools() on host with custom tool) ---
TEST_F(McpHostTest, ToolsAccessorIncludesCustomTools) {
    host.register_tool("custom", "Custom tool", {{"type", "object"}},
                       [](const nlohmann::json& args) { return args; });
    const auto& tools = host.tools();
    ASSERT_GE(tools.size(), 3u);
    bool has_custom = false;
    for (const auto& t : tools) {
        if (t.name == "custom") has_custom = true;
    }
    EXPECT_TRUE(has_custom);
}

} // namespace
} // namespace euxis::gateway
