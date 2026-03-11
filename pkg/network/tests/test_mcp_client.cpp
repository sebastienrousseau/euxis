#include <gtest/gtest.h>
#include "euxis/network/mcp_client.hpp"
#include <euxis/a2a/transport.hpp>
#include <euxis/a2a/message.hpp>

namespace euxis::network {
namespace {

class MockTransport : public euxis::a2a::ITransport {
public:
    std::function<std::expected<euxis::a2a::A2AMessage, euxis::a2a::TransportError>(const euxis::a2a::A2AMessage&)> on_send;

    auto send(const euxis::a2a::A2AMessage& msg) -> std::expected<euxis::a2a::A2AMessage, euxis::a2a::TransportError> override {
        if (on_send) return on_send(msg);
        return std::unexpected(euxis::a2a::TransportError::Timeout);
    }
};

TEST(McpClientTest, InitializeSuccess) {
    auto transport = std::make_shared<MockTransport>();
    transport->on_send = [](const euxis::a2a::A2AMessage& msg) -> std::expected<euxis::a2a::A2AMessage, euxis::a2a::TransportError> {
        nlohmann::json req = nlohmann::json::parse(msg.parts[0].content);
        nlohmann::json res = {
            {"jsonrpc", "2.0"},
            {"id", req["id"]},
            {"result", {{"protocolVersion", "2024-11-05"}}}
        };
        euxis::a2a::A2AMessage reply;
        reply.parts.push_back({.type = "text", .content = res.dump(), .mime_type = std::nullopt});
        return reply;
    };

    McpClient client(transport);
    auto res = client.initialize();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res.value()["protocolVersion"], "2024-11-05");
}

TEST(McpClientTest, TransportError) {
    auto transport = std::make_shared<MockTransport>();
    transport->on_send = [](const euxis::a2a::A2AMessage&) -> std::expected<euxis::a2a::A2AMessage, euxis::a2a::TransportError> {
        return std::unexpected(euxis::a2a::TransportError::ConnectionFailed);
    };

    McpClient client(transport);
    EXPECT_FALSE(client.list_tools().has_value());
}

TEST(McpClientTest, ServerErrorResponse) {
    auto transport = std::make_shared<MockTransport>();
    transport->on_send = [](const euxis::a2a::A2AMessage& msg) -> std::expected<euxis::a2a::A2AMessage, euxis::a2a::TransportError> {
        nlohmann::json req = nlohmann::json::parse(msg.parts[0].content);
        nlohmann::json res = {
            {"jsonrpc", "2.0"},
            {"id", req["id"]},
            {"error", {{"code", -32601}, {"message", "Method not found"}}}
        };
        euxis::a2a::A2AMessage reply;
        reply.parts.push_back({.type = "text", .content = res.dump(), .mime_type = std::nullopt});
        return reply;
    };

    McpClient client(transport);
    auto res = client.call_tool("unknown_tool", {});
    EXPECT_FALSE(res.has_value());
}

TEST(McpClientTest, InvalidJsonResponse) {
    auto transport = std::make_shared<MockTransport>();
    transport->on_send = [](const euxis::a2a::A2AMessage&) -> std::expected<euxis::a2a::A2AMessage, euxis::a2a::TransportError> {
        euxis::a2a::A2AMessage reply;
        reply.parts.push_back({.type = "text", .content = "this is not json", .mime_type = std::nullopt});
        return reply;
    };

    McpClient client(transport);
    EXPECT_FALSE(client.list_tools().has_value());
}

TEST(McpClientTest, EmptyResponseParts) {
    auto transport = std::make_shared<MockTransport>();
    transport->on_send = [](const euxis::a2a::A2AMessage&) -> std::expected<euxis::a2a::A2AMessage, euxis::a2a::TransportError> {
        euxis::a2a::A2AMessage reply;
        return reply;
    };

    McpClient client(transport);
    auto res = client.list_tools();
    EXPECT_FALSE(res.has_value());
}

} // namespace
} // namespace euxis::network
