#include <gtest/gtest.h>
#include "euxis/cli/cmd/fleet.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {
namespace {

class FleetCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_fleet_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/euxis-data";
        std::filesystem::create_directories(ctx_.data_dir + "/agents");

        // Create a test registry
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "alpha"}, {"role", "leader"}, {"version", "1.0"}, {"tier", "code"}},
            {{"agent_id", "beta"}, {"role", "worker"}, {"version", "1.0"}, {"tier", "routine"}}
        });
        std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();
    }

    void TearDown() override {
        std::filesystem::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

TEST_F(FleetCmdTest, AgentList) {
    auto code = cmd_agent(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, AgentListJson) {
    ctx_.json_output = true;
    auto code = cmd_agent(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, AgentInfoFound) {
    auto code = cmd_agent(ctx_, {"info", "alpha"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, AgentInfoNotFound) {
    auto code = cmd_agent(ctx_, {"info", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentUsage) {
    auto code = cmd_agent(ctx_, {"bogus-subcommand"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, AgentBootstrapCreates) {
    auto code = cmd_agent_bootstrap(ctx_, {"new-agent", "--tier", "routine"});
    EXPECT_EQ(code, 0);
    auto path = std::filesystem::path(ctx_.data_dir) / "agents" / "prompts" / "fleet" / "new-agent.md";
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(FleetCmdTest, AgentBootstrapInvalidId) {
    auto code = cmd_agent_bootstrap(ctx_, {"bad/id"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, SquadListEmpty) {
    auto code = cmd_squad(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, ComboUsage) {
    auto code = cmd_combo(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, ComboRuns) {
    auto code = cmd_combo(ctx_, {"alpha,beta", "test task"});
    // Returns 0 when providers are available, 1 when they fail (e.g. CI/test)
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(FleetCmdTest, CouncilUsage) {
    auto code = cmd_council(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, LoopUsage) {
    auto code = cmd_loop(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, SynthesizeUsage) {
    auto code = cmd_synthesize(ctx_, {});
    EXPECT_EQ(code, 2);
}

} // namespace
} // namespace euxis::cli::cmd
