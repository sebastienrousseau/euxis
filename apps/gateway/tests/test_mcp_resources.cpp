#include <gtest/gtest.h>
#include "euxis/gateway/mcp.hpp"
#include "euxis/gateway/mcp_fleet_tools.hpp"

#include <filesystem>
#include <fstream>

namespace euxis::gateway {

class McpResourceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_home_ = "/tmp/euxis_test_mcp_res_" + std::to_string(getpid());
        std::filesystem::create_directories(test_home_ + "/data/runtime/sessions");
        std::filesystem::create_directories(test_home_ + "/data/agents");
        std::filesystem::create_directories(test_home_ + "/data/config/playbooks");

        host_.emplace();
        register_fleet_tools(*host_, test_home_);

        // Initialize the host
        host_->handle_request({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
            {"params", {{"protocolVersion", "2024-11-05"}, {"capabilities", {}},
                         {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}}}});
        host_->handle_request({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    }

    void TearDown() override {
        std::filesystem::remove_all(test_home_);
    }

    std::string test_home_;
    std::optional<McpHost> host_;
};

TEST_F(McpResourceTest, ResourcesRegistered) {
    EXPECT_GE(host_->resources().size(), 6u);
}

TEST_F(McpResourceTest, ResourcesList) {
    auto resp = host_->handle_request({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "resources/list"}});
    EXPECT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].contains("resources"));
    EXPECT_GE(resp["result"]["resources"].size(), 6u);
}

TEST_F(McpResourceTest, ReadLatestVerdict) {
    // Create a test verdict
    nlohmann::json verdict = {{"verdict", "TRUSTED"}, {"confidence", 90}};
    std::ofstream(test_home_ + "/data/runtime/sessions/latest_verdict.json") << verdict.dump();

    auto resp = host_->handle_request({
        {"jsonrpc", "2.0"}, {"id", 3}, {"method", "resources/read"},
        {"params", {{"uri", "euxis://verdict/latest"}}}
    });
    EXPECT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].contains("contents"));
}

TEST_F(McpResourceTest, ReadUnknownUriErrors) {
    auto resp = host_->handle_request({
        {"jsonrpc", "2.0"}, {"id", 4}, {"method", "resources/read"},
        {"params", {{"uri", "euxis://nonexistent"}}}
    });
    EXPECT_TRUE(resp.contains("error"));
}

TEST_F(McpResourceTest, ReadAgentRegistry) {
    nlohmann::json reg = {{"agents", nlohmann::json::array()}};
    std::ofstream(test_home_ + "/data/agents/registry.json") << reg.dump();

    auto resp = host_->handle_request({
        {"jsonrpc", "2.0"}, {"id", 5}, {"method", "resources/read"},
        {"params", {{"uri", "euxis://agents"}}}
    });
    EXPECT_TRUE(resp.contains("result"));
}

TEST_F(McpResourceTest, ReadPlaybooks) {
    // Create a test playbook
    std::ofstream(test_home_ + "/data/config/playbooks/test-pb.json") << "{}";

    auto resp = host_->handle_request({
        {"jsonrpc", "2.0"}, {"id", 6}, {"method", "resources/read"},
        {"params", {{"uri", "euxis://playbooks"}}}
    });
    EXPECT_TRUE(resp.contains("result"));
}

TEST_F(McpResourceTest, ResourceHasFields) {
    auto resp = host_->handle_request({{"jsonrpc", "2.0"}, {"id", 7}, {"method", "resources/list"}});
    auto& resources = resp["result"]["resources"];
    for (const auto& r : resources) {
        EXPECT_TRUE(r.contains("uri"));
        EXPECT_TRUE(r.contains("name"));
    }
}

TEST_F(McpResourceTest, ReadVerdictHistory) {
    // Create history
    std::ofstream hf(test_home_ + "/data/runtime/sessions/history.jsonl");
    hf << nlohmann::json({{"verdict", "TRUSTED"}, {"confidence", 80}}).dump() << "\n";
    hf << nlohmann::json({{"verdict", "BLOCKED"}, {"confidence", 30}}).dump() << "\n";
    hf.close();

    auto resp = host_->handle_request({
        {"jsonrpc", "2.0"}, {"id", 8}, {"method", "resources/read"},
        {"params", {{"uri", "euxis://verdict/history"}}}
    });
    EXPECT_TRUE(resp.contains("result"));
}

} // namespace euxis::gateway
