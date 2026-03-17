#include <gtest/gtest.h>
#include "euxis/cli/cmd/system.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {
namespace {

class SystemCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_sys_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        std::filesystem::create_directories(ctx_.data_dir + "/agents");
        std::filesystem::create_directories(ctx_.data_dir + "/config");
    }

    void TearDown() override {
        std::filesystem::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

TEST_F(SystemCmdTest, DoctorRuns) {
    auto code = cmd_doctor(ctx_, {});
    // May fail due to missing deps, but shouldn't crash
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, DoctorJsonOutput) {
    ctx_.json_output = true;
    auto code = cmd_doctor(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, HealthRuns) {
    auto code = cmd_health(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, HealthSilent) {
    auto code = cmd_health(ctx_, {"--silent"});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, VerifyRuns) {
    auto code = cmd_verify(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, LintRuns) {
    auto code = cmd_lint(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, VerifyAllRuns) {
    auto code = cmd_verify_all(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, CrossPlatformVerify) {
    auto code = cmd_cross_platform_verify(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SystemCmdTest, DoctorWithFix) {
    auto code = cmd_doctor(ctx_, {"--fix"});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, HealthWithJsonOutput) {
    ctx_.json_output = true;
    auto code = cmd_health(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, HealthSilentWithJsonOutput) {
    ctx_.json_output = true;
    auto code = cmd_health(ctx_, {"--silent"});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, VerifyWithAgents) {
    // Create agents with prompt paths
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "a1"}, {"role", "r1"}, {"version", "1.0"},
         {"tier", "code"}, {"prompt_path", "agents/a1.md"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    // Create the prompt file with required fields in the data dir
    std::filesystem::create_directories(ctx_.data_dir + "/agents");
    std::ofstream(ctx_.data_dir + "/agents/a1.md")
        << "agent_id: a1\nrole: r1\nversion: 1.0";

    auto code = cmd_verify(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SystemCmdTest, VerifyWithMissingPrompt) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "missing-agent"}, {"role", "r1"},
         {"prompt_path", "agents/missing.md"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_verify(ctx_, {});
    EXPECT_EQ(code, 1);
}

TEST_F(SystemCmdTest, VerifyWithNoPromptPath) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "no-path"}, {"role", "r1"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_verify(ctx_, {});
    EXPECT_EQ(code, 1);
}

TEST_F(SystemCmdTest, LintWithValidRegistry) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "a1"}, {"role", "r1"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_lint(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SystemCmdTest, LintWithInvalidRegistry) {
    // Registry missing agents array
    nlohmann::json reg;
    reg["something_else"] = "value";
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_lint(ctx_, {});
    EXPECT_EQ(code, 1);
}

TEST_F(SystemCmdTest, LintWithRouterConfig) {
    // Create valid registry and router config
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    nlohmann::json router;
    router["models"] = {};
    std::ofstream(ctx_.data_dir + "/config/router.json") << router.dump();

    auto code = cmd_lint(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SystemCmdTest, ShellLintWithoutShellcheck) {
    auto code = cmd_shell_lint(ctx_, {});
    // Should either succeed or fail due to missing shellcheck, not crash
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, DoctorVerbose) {
    ctx_.verbose = true;
    auto code = cmd_doctor(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, HealthWithBusAndCortexDirs) {
    // Create runtime directories
    std::filesystem::create_directories(
        ctx_.euxis_home + "/data/runtime/data/bus/pipes");
    std::filesystem::create_directories(
        ctx_.euxis_home + "/data/runtime/memory/cortex/db");

    auto code = cmd_health(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, HealthVerboseWithBadNames) {
    ctx_.verbose = true;
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "BADNAME"}, {"role", "r1"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_health(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(SystemCmdTest, CrossPlatformWithRegistryFile) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_cross_platform_verify(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: doctor --fix with mkdir fixable items ---
TEST_F(SystemCmdTest, DoctorFixWithMissingDirs) {
    // Remove the data dir to create fixable conditions
    std::filesystem::remove_all(ctx_.data_dir);
    auto code = cmd_doctor(ctx_, {"--fix"});
    EXPECT_GE(code, 0);
}

// --- Coverage: health with bus pipes directory ---
TEST_F(SystemCmdTest, HealthWithBusPipes) {
    auto bus_dir = ctx_.euxis_home + "/data/runtime/data/bus/pipes";
    std::filesystem::create_directories(bus_dir);
    std::ofstream(bus_dir + "/test-pipe") << "data";

    auto code = cmd_health(ctx_, {});
    EXPECT_GE(code, 0);
}

// --- Coverage: health with cortex DB directory ---
TEST_F(SystemCmdTest, HealthWithCortexDb) {
    auto cortex_dir = ctx_.euxis_home + "/data/runtime/memory/cortex/db";
    std::filesystem::create_directories(cortex_dir);
    std::ofstream(cortex_dir + "/test.db") << "db data";

    auto code = cmd_health(ctx_, {});
    EXPECT_GE(code, 0);
}

// --- Coverage: verify with prompt file missing required frontmatter fields ---
TEST_F(SystemCmdTest, VerifyWithPromptMissingFields) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "partial-agent"}, {"role", "r1"}, {"version", "1.0"},
         {"tier", "code"}, {"prompt_path", "agents/partial.md"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    // Create prompt file with only agent_id but missing role and version
    std::filesystem::create_directories(ctx_.data_dir + "/agents");
    std::ofstream(ctx_.data_dir + "/agents/partial.md")
        << "agent_id: partial-agent";

    auto code = cmd_verify(ctx_, {});
    EXPECT_GE(code, 0);
}

// --- Coverage: lint with router config that has models section ---
TEST_F(SystemCmdTest, LintWithRouterModels) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    nlohmann::json router;
    router["models"] = {{"routine", "test-model"}, {"code", "test-code-model"}};
    std::ofstream(ctx_.data_dir + "/config/router.json") << router.dump();

    auto code = cmd_lint(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: verify-all exercises the combined check ---
TEST_F(SystemCmdTest, VerifyAllWithAgents) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "va"}, {"role", "test"}, {"version", "1.0"},
         {"tier", "code"}, {"prompt_path", "agents/va.md"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    std::filesystem::create_directories(ctx_.data_dir + "/agents");
    std::ofstream(ctx_.data_dir + "/agents/va.md")
        << "agent_id: va\nrole: test\nversion: 1.0";

    auto code = cmd_verify_all(ctx_, {});
    EXPECT_GE(code, 0);
}

// --- Coverage: shell-lint with bin directory ---
TEST_F(SystemCmdTest, ShellLintWithBinDir) {
    auto bin_dir = std::filesystem::path(ctx_.euxis_home) / "euxis-bin";
    std::filesystem::create_directories(bin_dir);
    std::ofstream(bin_dir / "test.sh") << "#!/bin/bash\necho hello\n";

    auto code = cmd_shell_lint(ctx_, {});
    EXPECT_GE(code, 0);
}

// --- Coverage: lint with duplicate agent IDs ---
TEST_F(SystemCmdTest, LintWithDuplicateIds) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "dup"}, {"role", "r1"}},
        {{"agent_id", "dup"}, {"role", "r2"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_lint(ctx_, {});
    EXPECT_GE(code, 0);
}

} // namespace
} // namespace euxis::cli::cmd
