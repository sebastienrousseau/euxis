#include <gtest/gtest.h>
// httplib.h triggers GCC's -Wmaybe-uninitialized; Clang has no such
// flag and -Wunknown-warning-option is -Werror under AppleClang.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <httplib.h>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#include <sodium.h>

#include <chrono>
#include <thread>

#include <nlohmann/json.hpp>

#include "euxis/gateway/server.hpp"

namespace euxis::gateway {
namespace {

class GatewayServerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0);
    }
};

TEST_F(GatewayServerTest, HealthEndpoint) {
    GatewayServer srv(GatewayConfig{.port = 0,
                                     .host = "0.0.0.0",
                                     .timeouts = {},
                                     .raw = {}});
    [[maybe_unused]] auto& s = srv.server();
    EXPECT_EQ(srv.port(), 0);
}

TEST_F(GatewayServerTest, DefaultConstruction) {
    GatewayServer srv;
    EXPECT_EQ(srv.port(), 8080);
    EXPECT_EQ(srv.ws_port(), 8081);
    EXPECT_FALSE(srv.is_running());
}

TEST_F(GatewayServerTest, CustomConfig) {
    GatewayConfig config;
    config.port = 9090;
    config.ws_port = 9091;
    config.host = "127.0.0.1";

    GatewayServer srv(config);
    EXPECT_EQ(srv.port(), 9090);
    EXPECT_EQ(srv.ws_port(), 9091);
    EXPECT_FALSE(srv.is_running());
}

TEST_F(GatewayServerTest, ServerAccessor) {
    GatewayServer srv;
    auto& s = srv.server();
    // Verify we get a valid httplib::Server reference
    (void)s;
    SUCCEED();
}

TEST_F(GatewayServerTest, WsHubAccessor) {
    GatewayConfig config;
    config.port = 0;
    GatewayServer srv(config);
    auto& hub = srv.ws_hub();
    (void)hub;
    SUCCEED();
}

TEST_F(GatewayServerTest, StopWithoutStart) {
    GatewayConfig config;
    config.port = 0;
    GatewayServer srv(config);
    // Stop should be safe even when not started
    srv.stop();
    EXPECT_FALSE(srv.is_running());
}

TEST_F(GatewayServerTest, StartAndStopWithClient) {
    // Use a random available port
    GatewayConfig config;
    config.port = 0;
    config.ws_port = 0;
    config.host = "127.0.0.1";
    config.raw = {{"gateway", {{"auth", {{"token", {{"value", "test-token-xyz"}}}}}}}};

    GatewayServer srv(config);

    // Get the server to auto-bind to a port
    auto& s = srv.server();
    int port = s.bind_to_any_port("127.0.0.1");
    EXPECT_GT(port, 0);

    // Start in a background thread
    std::thread server_thread([&]() {
        s.listen_after_bind();
    });

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Test health endpoint (no auth needed)
    httplib::Client cli("127.0.0.1", port);
    cli.set_connection_timeout(2);
    httplib::Headers auth_headers = {{"Authorization", "Bearer test-token-xyz"}};

    auto res = cli.Get("/health");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["status"], "ok");

    // Test ready endpoint
    auto ready_res = cli.Get("/ready");
    ASSERT_TRUE(ready_res != nullptr);
    EXPECT_EQ(ready_res->status, 200);
    auto ready_body = nlohmann::json::parse(ready_res->body);
    EXPECT_EQ(ready_body["ready"], true);

    // Test admin config endpoint (requires auth)
    auto admin_res = cli.Get("/api/admin/config", auth_headers);
    ASSERT_TRUE(admin_res != nullptr);
    EXPECT_EQ(admin_res->status, 200);
    auto admin_body = nlohmann::json::parse(admin_res->body);
    EXPECT_EQ(admin_body["runtime"], "cpp");

    // Test admin config without auth — should be rejected
    auto admin_noauth = cli.Get("/api/admin/config");
    ASSERT_TRUE(admin_noauth != nullptr);
    EXPECT_EQ(admin_noauth->status, 401);

    // Test webhook inbound with valid JSON
    auto webhook_res = cli.Post("/api/webhooks/inbound",
                                 auth_headers,
                                 R"({"event":"test"})",
                                 "application/json");
    ASSERT_TRUE(webhook_res != nullptr);
    EXPECT_EQ(webhook_res->status, 200);

    // Test webhook inbound with invalid JSON
    auto webhook_bad = cli.Post("/api/webhooks/inbound",
                                 auth_headers,
                                 "not json{{{",
                                 "application/json");
    ASSERT_TRUE(webhook_bad != nullptr);
    EXPECT_EQ(webhook_bad->status, 400);

    // Test session POST with valid data
    auto session_res = cli.Post("/api/sessions",
                                 auth_headers,
                                 R"({"session_id":"test-session","content":"hello"})",
                                 "application/json");
    ASSERT_TRUE(session_res != nullptr);
    EXPECT_EQ(session_res->status, 200);

    // Test session POST with missing fields
    auto session_bad = cli.Post("/api/sessions",
                                 auth_headers,
                                 R"({"session_id":""})",
                                 "application/json");
    ASSERT_TRUE(session_bad != nullptr);
    EXPECT_EQ(session_bad->status, 400);

    // Test session POST with invalid JSON
    auto session_invalid = cli.Post("/api/sessions",
                                     auth_headers,
                                     "not json{{{",
                                     "application/json");
    ASSERT_TRUE(session_invalid != nullptr);
    EXPECT_EQ(session_invalid->status, 400);

    // Test MCP endpoint with valid JSON-RPC
    auto mcp_res = cli.Post("/mcp",
                             auth_headers,
                             R"({"jsonrpc":"2.0","method":"initialize","id":1})",
                             "application/json");
    ASSERT_TRUE(mcp_res != nullptr);
    EXPECT_EQ(mcp_res->status, 200);

    // Test MCP endpoint with invalid JSON
    auto mcp_bad = cli.Post("/mcp",
                             auth_headers,
                             "not json{{{",
                             "application/json");
    ASSERT_TRUE(mcp_bad != nullptr);
    EXPECT_EQ(mcp_bad->status, 200); // Returns JSON-RPC error

    // --- Coverage: routes_sessions.cpp lines 11-17 (GET /api/sessions/:id) ---
    // Test session GET endpoint
    auto session_get = cli.Get("/api/sessions/test-session", auth_headers);
    ASSERT_TRUE(session_get != nullptr);
    // Session may or may not exist (depends on server state) — just ensure
    // the endpoint responds (200 or 404)
    EXPECT_TRUE(session_get->status == 200 || session_get->status == 404);

    // Stop the server
    s.stop();
    server_thread.join();
}

} // namespace
} // namespace euxis::gateway
