#include <gtest/gtest.h>
#include "euxis/gateway/mcp_fleet_tools.hpp"
#include "euxis/gateway/mcp_stdio.hpp"

#include <sstream>

namespace euxis::gateway {

class McpFleetToolsTest : public ::testing::Test {
protected:
    void SetUp() override {
        host_.emplace();
        register_fleet_tools(*host_, "/tmp/euxis_test_mcp");
    }
    std::optional<McpHost> host_;
};

TEST_F(McpFleetToolsTest, ToolsRegistered) {
    EXPECT_GE(host_->tools().size(), 6u);
}

TEST_F(McpFleetToolsTest, CheckToolRegistered) {
    bool found = false;
    for (const auto& t : host_->tools()) {
        if (t.name == "euxis.check") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(McpFleetToolsTest, TriageToolRegistered) {
    bool found = false;
    for (const auto& t : host_->tools()) {
        if (t.name == "euxis.triage") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(McpFleetToolsTest, ReviewToolRegistered) {
    bool found = false;
    for (const auto& t : host_->tools()) {
        if (t.name == "euxis.review") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(McpFleetToolsTest, StatsToolRegistered) {
    bool found = false;
    for (const auto& t : host_->tools()) {
        if (t.name == "euxis.stats") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(McpFleetToolsTest, AgentListToolRegistered) {
    bool found = false;
    for (const auto& t : host_->tools()) {
        if (t.name == "euxis.agent_list") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(McpFleetToolsTest, PlaybookToolRegistered) {
    bool found = false;
    for (const auto& t : host_->tools()) {
        if (t.name == "euxis.playbook") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(McpFleetToolsTest, InitializeHandshake) {
    nlohmann::json init_req = {
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2024-11-05"}, {"capabilities", {}},
                     {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}}}
    };
    auto resp = host_->handle_request(init_req);
    EXPECT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].contains("protocolVersion"));
}

TEST_F(McpFleetToolsTest, ToolsList) {
    // Initialize first
    host_->handle_request({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2024-11-05"}, {"capabilities", {}},
                     {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}}}});
    host_->handle_request({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});

    nlohmann::json list_req = {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}};
    auto resp = host_->handle_request(list_req);
    EXPECT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].contains("tools"));
    EXPECT_GE(resp["result"]["tools"].size(), 6u);
}

// --- P0-1: Injection rejection tests ---

TEST_F(McpFleetToolsTest, CheckRejectsSemicolonInTarget) {
    // Initialize + notify
    host_->handle_request({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2024-11-05"}, {"capabilities", {}},
                     {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}}}});
    host_->handle_request({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});

    nlohmann::json call_req = {
        {"jsonrpc", "2.0"}, {"id", 10}, {"method", "tools/call"},
        {"params", {{"name", "euxis.check"}, {"arguments", {{"target", "/tmp; rm -rf /"}}}}}
    };
    auto resp = host_->handle_request(call_req);
    // The tool should return an error about unsafe argument
    ASSERT_TRUE(resp.contains("result"));
    auto content = resp["result"]["content"][0]["text"].get<std::string>();
    auto result_json = nlohmann::json::parse(content);
    EXPECT_TRUE(result_json.contains("error"));
    EXPECT_NE(result_json["error"].get<std::string>().find("unsafe"), std::string::npos);
}

TEST_F(McpFleetToolsTest, PlaybookRejectsCommandSubstitutionInGoal) {
    host_->handle_request({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2024-11-05"}, {"capabilities", {}},
                     {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}}}});
    host_->handle_request({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});

    nlohmann::json call_req = {
        {"jsonrpc", "2.0"}, {"id", 11}, {"method", "tools/call"},
        {"params", {{"name", "euxis.playbook"}, {"arguments", {
            {"manifest", "verify-everything"},
            {"goal", "$(cat /etc/passwd)"}
        }}}}
    };
    auto resp = host_->handle_request(call_req);
    ASSERT_TRUE(resp.contains("result"));
    auto content = resp["result"]["content"][0]["text"].get<std::string>();
    auto result_json = nlohmann::json::parse(content);
    EXPECT_TRUE(result_json.contains("error"));
    EXPECT_NE(result_json["error"].get<std::string>().find("unsafe"), std::string::npos);
}

// --- Stdio framing tests ---

TEST(McpStdioTest, WriteFrame) {
    std::stringstream out;
    nlohmann::json msg = {{"test", "value"}};
    McpStdioServer::write_frame(out, msg);

    auto output = out.str();
    EXPECT_TRUE(output.starts_with("Content-Length: "));
    EXPECT_NE(output.find("\r\n\r\n"), std::string::npos);
}

TEST(McpStdioTest, ReadFrame) {
    nlohmann::json msg = {{"hello", "world"}};
    std::string body = msg.dump();
    std::stringstream in;
    in << "Content-Length: " << body.size() << "\r\n\r\n" << body;

    auto parsed = McpStdioServer::read_frame(in);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed)["hello"], "world");
}

TEST(McpStdioTest, ReadFrameRoundTrip) {
    nlohmann::json msg = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "test"}};

    std::stringstream ss;
    McpStdioServer::write_frame(ss, msg);

    auto parsed = McpStdioServer::read_frame(ss);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed)["method"], "test");
}

TEST(McpStdioTest, ReadFrameEmpty) {
    std::stringstream in;
    auto parsed = McpStdioServer::read_frame(in);
    EXPECT_FALSE(parsed.has_value());
}

TEST(McpStdioTest, ProcessOne) {
    McpHost host;
    McpStdioServer server(host);

    nlohmann::json init_req = {
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2024-11-05"}, {"capabilities", {}},
                     {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}}}
    };

    std::stringstream in, out;
    McpStdioServer::write_frame(in, init_req);

    bool ok = server.process_one(in, out);
    EXPECT_TRUE(ok);

    auto resp = McpStdioServer::read_frame(out);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->contains("result"));
}

} // namespace euxis::gateway
