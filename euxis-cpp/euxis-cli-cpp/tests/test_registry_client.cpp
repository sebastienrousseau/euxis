#include <gtest/gtest.h>
#include "euxis/cli/registry_client.hpp"

#include <filesystem>
#include <fstream>

namespace euxis::cli {
namespace {

class RegistryClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/euxis_test_registry_" + std::to_string(getpid());
        auto agents_dir = std::filesystem::path(test_dir_) / "agents";
        std::filesystem::create_directories(agents_dir);

        // Create a minimal registry.json
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "test-agent"}, {"role", "tester"}, {"version", "1.0"},
             {"tier", "code"}, {"tags", {"test", "ci"}}, {"capabilities", {"testing"}}},
            {{"agent_id", "helper"}, {"role", "helper"}, {"version", "2.0"},
             {"tier", "routine"}, {"tags", {"utility"}}, {"capabilities", {"help"}}}
        });
        std::ofstream(agents_dir / "registry.json") << reg.dump(2);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(RegistryClientTest, HasJson) {
    RegistryClient reg(test_dir_);
    EXPECT_TRUE(reg.has_json());
}

TEST_F(RegistryClientTest, ListAgents) {
    RegistryClient reg(test_dir_);
    auto agents = reg.list_agents();
    EXPECT_EQ(agents.size(), 2u);
}

TEST_F(RegistryClientTest, GetAgent) {
    RegistryClient reg(test_dir_);
    auto agent = reg.get_agent("test-agent");
    ASSERT_TRUE(agent.has_value());
    EXPECT_EQ(agent->id, "test-agent");
    EXPECT_EQ(agent->role, "tester");
    EXPECT_EQ(agent->tier, "code");
}

TEST_F(RegistryClientTest, GetAgentNotFound) {
    RegistryClient reg(test_dir_);
    auto agent = reg.get_agent("nonexistent");
    EXPECT_FALSE(agent.has_value());
}

TEST_F(RegistryClientTest, AgentCount) {
    RegistryClient reg(test_dir_);
    EXPECT_EQ(reg.agent_count(), 2);
}

TEST_F(RegistryClientTest, FindByTag) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tag("test");
    EXPECT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents[0].id, "test-agent");
}

TEST_F(RegistryClientTest, FindByTier) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tier("routine");
    EXPECT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents[0].id, "helper");
}

TEST_F(RegistryClientTest, FindByCapability) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_capability("testing");
    EXPECT_EQ(agents.size(), 1u);
}

TEST_F(RegistryClientTest, RegisterPlugin) {
    RegistryClient reg(test_dir_);
    nlohmann::json manifest = {{"agent_id", "custom"}, {"role", "custom"}};
    EXPECT_TRUE(reg.register_plugin("custom-agent", manifest));

    auto meta = std::filesystem::path(test_dir_) / "config" / "plugins" / "custom-agent.json";
    EXPECT_TRUE(std::filesystem::exists(meta));
}

TEST_F(RegistryClientTest, RegisterPluginInvalidId) {
    RegistryClient reg(test_dir_);
    EXPECT_FALSE(reg.register_plugin("bad/agent", {}));
    EXPECT_FALSE(reg.register_plugin("bad agent", {}));
}

TEST_F(RegistryClientTest, UnregisterPlugin) {
    RegistryClient reg(test_dir_);
    reg.register_plugin("to-remove", {});
    EXPECT_TRUE(reg.unregister_plugin("to-remove"));
    EXPECT_FALSE(reg.unregister_plugin("to-remove")); // already removed
}

TEST_F(RegistryClientTest, EmptyDataDir) {
    std::string empty_dir = "/tmp/euxis_test_empty_" + std::to_string(getpid());
    std::filesystem::create_directories(empty_dir);
    RegistryClient reg(empty_dir);
    EXPECT_EQ(reg.agent_count(), 0);
    EXPECT_TRUE(reg.list_agents().empty());
    std::filesystem::remove_all(empty_dir);
}

} // namespace
} // namespace euxis::cli
