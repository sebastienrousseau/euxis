#include <gtest/gtest.h>
#include "euxis/cli/mcp_provider.hpp"

#include <filesystem>
#include <fstream>

namespace euxis::cli {
namespace {

namespace fs = std::filesystem;

class McpProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/euxis_test_mcp_provider_" + std::to_string(getpid());
        fs::create_directories(test_dir_ + "/config");
    }
    void TearDown() override {
        fs::remove_all(test_dir_);
    }
    std::string test_dir_;
};

TEST_F(McpProviderTest, LoadsEmptyConfig) {
    nlohmann::json cfg;
    cfg["mcp_servers"] = nlohmann::json::object();
    std::ofstream(test_dir_ + "/config/router.json") << cfg.dump();

    McpProviderBridge bridge(test_dir_);
    EXPECT_TRUE(bridge.server_names().empty());
}

TEST_F(McpProviderTest, LoadsServerConfig) {
    nlohmann::json cfg;
    cfg["mcp_servers"] = {
        {"test-server", {{"command", "echo"}, {"args", {"hello"}}, {"timeout", 5}}}
    };
    std::ofstream(test_dir_ + "/config/router.json") << cfg.dump();

    McpProviderBridge bridge(test_dir_);
    EXPECT_TRUE(bridge.has_server("test-server"));
    EXPECT_FALSE(bridge.has_server("nonexistent"));
}

TEST_F(McpProviderTest, UnknownServerError) {
    nlohmann::json cfg;
    cfg["mcp_servers"] = nlohmann::json::object();
    std::ofstream(test_dir_ + "/config/router.json") << cfg.dump();

    McpProviderBridge bridge(test_dir_);
    auto resp = bridge.execute("nonexistent", "tool", {});
    EXPECT_FALSE(resp.success);
    EXPECT_NE(resp.error.find("unknown"), std::string::npos);
}

TEST_F(McpProviderTest, NoConfigFile) {
    McpProviderBridge bridge(test_dir_);
    EXPECT_TRUE(bridge.server_names().empty());
}

TEST_F(McpProviderTest, ServerWithNoCommand) {
    nlohmann::json cfg;
    cfg["mcp_servers"] = {
        {"empty-server", {{"timeout", 5}}}
    };
    std::ofstream(test_dir_ + "/config/router.json") << cfg.dump();

    McpProviderBridge bridge(test_dir_);
    auto resp = bridge.execute("empty-server", "tool", {});
    EXPECT_FALSE(resp.success);
}

TEST_F(McpProviderTest, ServerNames) {
    nlohmann::json cfg;
    cfg["mcp_servers"] = {
        {"server-a", {{"command", "a"}}},
        {"server-b", {{"command", "b"}}}
    };
    std::ofstream(test_dir_ + "/config/router.json") << cfg.dump();

    McpProviderBridge bridge(test_dir_);
    auto names = bridge.server_names();
    EXPECT_EQ(names.size(), 2u);
}

TEST_F(McpProviderTest, ConfigWithEnvVars) {
    nlohmann::json cfg;
    cfg["mcp_servers"] = {
        {"env-server", {{"command", "echo"}, {"env", {"FOO=bar", "BAZ=qux"}}}}
    };
    std::ofstream(test_dir_ + "/config/router.json") << cfg.dump();

    McpProviderBridge bridge(test_dir_);
    EXPECT_TRUE(bridge.has_server("env-server"));
}

TEST_F(McpProviderTest, ConfigWithArgs) {
    nlohmann::json cfg;
    cfg["mcp_servers"] = {
        {"arg-server", {{"command", "node"}, {"args", {"server.js", "--stdio"}}}}
    };
    std::ofstream(test_dir_ + "/config/router.json") << cfg.dump();

    McpProviderBridge bridge(test_dir_);
    EXPECT_TRUE(bridge.has_server("arg-server"));
}

} // namespace
} // namespace euxis::cli
