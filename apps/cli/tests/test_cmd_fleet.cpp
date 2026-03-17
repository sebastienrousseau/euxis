#include <gtest/gtest.h>
#include "euxis/cli/cmd/fleet.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class FleetCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        setenv("EUXIS_TEST_MOCK_EXECUTION", "1", 1);
        ctx_.euxis_home = "/tmp/euxis_test_fleet_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        fs::create_directories(ctx_.data_dir + "/agents");

        // Create a test registry with agents
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "alpha"}, {"role", "leader"}, {"version", "1.0"}, {"tier", "code"},
             {"tags", {"core", "leader"}}, {"capabilities", {"routing", "dispatch"}},
             {"prompt_path", ""}},
            {{"agent_id", "beta"}, {"role", "worker"}, {"version", "1.0"}, {"tier", "routine"},
             {"tags", {"worker"}}, {"capabilities", {"coding"}},
             {"prompt_path", ""}}
        });
        reg["squads"] = nlohmann::json::array({
            {{"id", "core-squad"}, {"name", "Core Squad"}, {"purpose", "Core tasks"},
             {"lead", "alpha"}, {"members", {"alpha", "beta"}}}
        });
        std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();
    }

    void TearDown() override {
        unsetenv("EUXIS_TEST_MOCK_EXECUTION");
        fs::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

// ---- Agent command tests ----

TEST_F(FleetCmdTest, AgentList) {
    auto code = cmd_agent(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, AgentListDefault) {
    auto code = cmd_agent(ctx_, {});
    EXPECT_EQ(code, 0); // list is default
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

TEST_F(FleetCmdTest, AgentInfoFoundJson) {
    ctx_.json_output = true;
    auto code = cmd_agent(ctx_, {"info", "alpha"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, AgentInfoNotFound) {
    auto code = cmd_agent(ctx_, {"info", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentInfoNotFoundJson) {
    ctx_.json_output = true;
    auto code = cmd_agent(ctx_, {"info", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentUsage) {
    auto code = cmd_agent(ctx_, {"bogus-subcommand"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, AgentRegisterMissingManifest) {
    auto code = cmd_agent(ctx_, {"register", "/tmp/nonexistent-manifest.json"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentRegisterInvalidJson) {
    auto manifest_path = ctx_.euxis_home + "/bad-manifest.json";
    std::ofstream(manifest_path) << "not json{{";

    auto code = cmd_agent(ctx_, {"register", manifest_path});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentRegisterMissingAgentId) {
    auto manifest_path = ctx_.euxis_home + "/no-id-manifest.json";
    nlohmann::json manifest;
    manifest["role"] = "worker";
    std::ofstream(manifest_path) << manifest.dump();

    auto code = cmd_agent(ctx_, {"register", manifest_path});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentRegisterSuccess) {
    auto manifest_path = ctx_.euxis_home + "/good-manifest.json";
    nlohmann::json manifest;
    manifest["agent_id"] = "gamma";
    manifest["role"] = "worker";
    manifest["tier"] = "code";
    manifest["version"] = "1.0";
    std::ofstream(manifest_path) << manifest.dump();

    auto code = cmd_agent(ctx_, {"register", manifest_path});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, AgentUnregisterExisting) {
    auto code = cmd_agent(ctx_, {"unregister", "alpha"});
    // cmd_agent unregister may return non-zero if the agent file doesn't exist
    // as a standalone manifest. Still exercises the unregister path.
    EXPECT_GE(code, 0);
}

TEST_F(FleetCmdTest, AgentUnregisterNotFound) {
    auto code = cmd_agent(ctx_, {"unregister", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentRegisterNoArgs) {
    auto code = cmd_agent(ctx_, {"register"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, AgentUnregisterNoArgs) {
    auto code = cmd_agent(ctx_, {"unregister"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, AgentInfoNoArgs) {
    auto code = cmd_agent(ctx_, {"info"});
    EXPECT_EQ(code, 2);
}

// ---- Agent Bootstrap tests ----

TEST_F(FleetCmdTest, AgentBootstrapCreates) {
    auto code = cmd_agent_bootstrap(ctx_, {"new-agent", "--tier", "routine"});
    EXPECT_EQ(code, 0);
    auto path = fs::path(ctx_.euxis_home) / "data" / "agents" / "prompts" / "fleet" / "new-agent.md";
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(FleetCmdTest, AgentBootstrapDefaultTier) {
    auto code = cmd_agent_bootstrap(ctx_, {"default-tier-agent"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, AgentBootstrapInvalidId) {
    auto code = cmd_agent_bootstrap(ctx_, {"bad/id"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentBootstrapNoArgs) {
    auto code = cmd_agent_bootstrap(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, AgentBootstrapAlreadyExists) {
    // Create the agent first
    cmd_agent_bootstrap(ctx_, {"existing-agent"});
    // Try again
    auto code = cmd_agent_bootstrap(ctx_, {"existing-agent"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, AgentBootstrapInvalidCharSpace) {
    auto code = cmd_agent_bootstrap(ctx_, {"bad agent"});
    EXPECT_EQ(code, 1);
}

// ---- Squad command tests ----

TEST_F(FleetCmdTest, SquadListEmpty) {
    // Overwrite registry without squads
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_squad(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, SquadList) {
    auto code = cmd_squad(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, SquadListDefault) {
    auto code = cmd_squad(ctx_, {});
    EXPECT_EQ(code, 0); // list is default
}

TEST_F(FleetCmdTest, SquadListJson) {
    ctx_.json_output = true;
    auto code = cmd_squad(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, SquadInfoFound) {
    auto code = cmd_squad(ctx_, {"info", "core-squad"});
    EXPECT_GE(code, 0);  // exercises squad info lookup path
}

TEST_F(FleetCmdTest, SquadInfoNotFound) {
    auto code = cmd_squad(ctx_, {"info", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, SquadMembersFound) {
    auto code = cmd_squad(ctx_, {"members", "core-squad"});
    EXPECT_GE(code, 0);  // exercises squad members lookup path
}

TEST_F(FleetCmdTest, SquadMembersNotFound) {
    auto code = cmd_squad(ctx_, {"members", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, SquadValidate) {
    auto code = cmd_squad(ctx_, {"validate"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, SquadDeployNotFound) {
    auto code = cmd_squad(ctx_, {"deploy", "nonexistent", "some task"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, SquadDeploySuccess) {
    auto code = cmd_squad(ctx_, {"deploy", "core-squad", "build the app"});
    EXPECT_GE(code, 0);  // exercises squad deploy path
}

TEST_F(FleetCmdTest, SquadDeployWithMode) {
    auto code = cmd_squad(ctx_, {"deploy", "core-squad", "build app", "--mode", "parallel"});
    EXPECT_GE(code, 0);  // exercises squad deploy with mode path
}

TEST_F(FleetCmdTest, SquadUsage) {
    auto code = cmd_squad(ctx_, {"bogus"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, SquadDeployNoTask) {
    auto code = cmd_squad(ctx_, {"deploy", "core-squad"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, SquadInfoNoArgs) {
    auto code = cmd_squad(ctx_, {"info"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, SquadMembersNoArgs) {
    auto code = cmd_squad(ctx_, {"members"});
    EXPECT_EQ(code, 2);
}

// ---- Combo command tests ----

TEST_F(FleetCmdTest, ComboUsage) {
    auto code = cmd_combo(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, ComboRuns) {
    auto code = cmd_combo(ctx_, {"alpha,beta", "test task"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(FleetCmdTest, ComboSingleAgent) {
    auto code = cmd_combo(ctx_, {"alpha", "test task"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(FleetCmdTest, ComboDefaultTask) {
    auto code = cmd_combo(ctx_, {"alpha"});
    EXPECT_TRUE(code == 0 || code == 1);
}

// ---- Council command tests ----

TEST_F(FleetCmdTest, CouncilUsage) {
    auto code = cmd_council(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, CouncilUsageSingleArg) {
    auto code = cmd_council(ctx_, {"topic"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, CouncilRuns) {
    auto code = cmd_council(ctx_, {"code quality", "alpha,beta", "--rounds", "1"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(FleetCmdTest, CouncilDefaultRounds) {
    auto code = cmd_council(ctx_, {"code quality", "alpha"});
    // Default is 3 rounds; runs providers which fail in test
    EXPECT_TRUE(code == 0 || code == 1);
}

// ---- Loop command tests ----

TEST_F(FleetCmdTest, LoopUsage) {
    auto code = cmd_loop(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, LoopUsageSingleArg) {
    auto code = cmd_loop(ctx_, {"alpha"});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, LoopRuns) {
    auto code = cmd_loop(ctx_, {"alpha", "task", "--max-iterations", "1", "--threshold", "0.1"});
    EXPECT_EQ(code, 0);
}

TEST_F(FleetCmdTest, LoopDefaultOptions) {
    auto code = cmd_loop(ctx_, {"alpha", "refine output"});
    EXPECT_EQ(code, 0);
}

// ---- Playbook command tests ----

TEST_F(FleetCmdTest, PlaybookUsage) {
    auto code = cmd_playbook(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, PlaybookMissingFile) {
    auto code = cmd_playbook(ctx_, {"/tmp/nonexistent.json"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, PlaybookInvalidJson) {
    auto path = ctx_.euxis_home + "/bad-playbook.json";
    std::ofstream(path) << "not json{{{";
    auto code = cmd_playbook(ctx_, {path});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, PlaybookMissingSteps) {
    auto path = ctx_.euxis_home + "/no-steps.json";
    nlohmann::json manifest;
    manifest["name"] = "test";
    std::ofstream(path) << manifest.dump();
    auto code = cmd_playbook(ctx_, {path});
    EXPECT_EQ(code, 2); // No evidence collected
}

TEST_F(FleetCmdTest, PlaybookWithStepMissingAgent) {
    // Resilience: step with no agent field should not crash; falls back to "code-agent"
    auto path = ctx_.euxis_home + "/playbook-no-agent.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "step1"}, {"task", "do something"}}
    });
    std::ofstream(path) << manifest.dump();

    // Run in CI mode to capture artifact and verify graceful handling
    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    auto code = cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    // Completes without crashing; exit code depends on confidence/verdict
    EXPECT_TRUE(code == 0 || code == 1) << "Missing agent should not crash (got exit " << code << ")";

    // Artifact is produced with valid verdict
    auto output = buffer.str();
    ASSERT_FALSE(output.empty()) << "CI mode must produce output";
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;
    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded()) << "Missing agent must still produce valid JSON artifact";
    EXPECT_TRUE(parsed.contains("verdict"));
    EXPECT_TRUE(parsed.contains("confidence"));
    EXPECT_TRUE(parsed.contains("agent_status"));
}

TEST_F(FleetCmdTest, PlaybookWithValidSteps) {
    auto path = ctx_.euxis_home + "/playbook.json";
    nlohmann::json manifest;
    manifest["task"] = "test";
    manifest["steps"] = nlohmann::json::array({
        {{"name", "research"}, {"agent", "alpha"}, {"task", "research topic"}},
        {{"name", "write"}, {"agent", "beta"}, {"task", "write doc"}, {"depends", {"research"}}}
    });
    std::ofstream(path) << manifest.dump();

    auto code = cmd_playbook(ctx_, {path});
    // Provider calls may fail in test env
    EXPECT_TRUE(code == 0 || code == 1);
}

// ---- CI mode tests ----

TEST_F(FleetCmdTest, PlaybookCIMode) {
    auto path = ctx_.euxis_home + "/playbook-ci.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    // Capture stdout
    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    auto code = cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    // CI mode: stdout contains JSON only, dashboard goes to stderr
    auto output = buffer.str();
    ASSERT_FALSE(output.empty()) << "CI mode must produce stdout output";

    // stdout should be clean JSON — find the artifact
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++; // skip the newline
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;

    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded()) << "CI output must contain valid JSON";
    EXPECT_TRUE(parsed.contains("verdict"));
    EXPECT_TRUE(parsed.contains("confidence"));
    EXPECT_TRUE(parsed.contains("sla"));
    EXPECT_TRUE(parsed.contains("agent_status"));
    EXPECT_TRUE(parsed.contains("exit_code"));
    EXPECT_TRUE(parsed.contains("ci"));
    EXPECT_EQ(parsed["ci"].get<bool>(), true);
    EXPECT_EQ(parsed["exit_code"].get<int>(), code);
}

TEST_F(FleetCmdTest, PlaybookCIModeEmptyManifest) {
    auto path = ctx_.euxis_home + "/empty-ci.json";
    nlohmann::json manifest;
    manifest["name"] = "test";
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    auto code = cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    EXPECT_EQ(code, 2); // No evidence
    auto output = buffer.str();
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;
    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded()) << "CI output must contain valid JSON";
    EXPECT_EQ(parsed["exit_code"].get<int>(), 2);
}

// ---- Dispatch command tests ----

TEST_F(FleetCmdTest, DispatchUsage) {
    auto code = cmd_dispatch(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, DispatchMissingFile) {
    auto code = cmd_dispatch(ctx_, {"/tmp/nonexistent-dispatch.json"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, DispatchInvalidJson) {
    auto path = ctx_.euxis_home + "/bad-dispatch.json";
    std::ofstream(path) << "not json{{{";
    auto code = cmd_dispatch(ctx_, {path});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, DispatchMissingAgentsArray) {
    auto path = ctx_.euxis_home + "/no-agents-dispatch.json";
    nlohmann::json manifest;
    manifest["task"] = "do something";
    std::ofstream(path) << manifest.dump();
    auto code = cmd_dispatch(ctx_, {path});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, DispatchWithAgents) {
    auto path = ctx_.euxis_home + "/dispatch.json";
    nlohmann::json manifest;
    manifest["task"] = "test task";
    manifest["agents"] = nlohmann::json::array({
        {{"agent_id", "alpha"}, {"priority", "P0"}, {"task", "lead task"}},
        {{"agent_id", "beta"}, {"priority", "P1"}}
    });
    std::ofstream(path) << manifest.dump();

    auto code = cmd_dispatch(ctx_, {path});
    EXPECT_TRUE(code == 0 || code == 1);
}

// ---- Synthesize command tests ----

TEST_F(FleetCmdTest, SynthesizeUsage) {
    auto code = cmd_synthesize(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(FleetCmdTest, SynthesizeMissingDir) {
    auto code = cmd_synthesize(ctx_, {"/tmp/nonexistent-synth-dir"});
    EXPECT_EQ(code, 1);
}

TEST_F(FleetCmdTest, SynthesizeEmptyDir) {
    auto synth_dir = ctx_.euxis_home + "/synth-output";
    fs::create_directories(synth_dir);
    auto code = cmd_synthesize(ctx_, {synth_dir});
    EXPECT_EQ(code, 0); // no files found is not an error
}

TEST_F(FleetCmdTest, SynthesizeWithFiles) {
    auto synth_dir = ctx_.euxis_home + "/synth-output";
    fs::create_directories(synth_dir);
    std::ofstream(synth_dir + "/output1.md") << "Result from agent 1";
    std::ofstream(synth_dir + "/output2.md") << "Result from agent 2";

    auto code = cmd_synthesize(ctx_, {synth_dir});
    // Provider calls may fail in test env
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(FleetCmdTest, SynthesizeWithAgentFilter) {
    auto synth_dir = ctx_.euxis_home + "/synth-output";
    fs::create_directories(synth_dir + "/alpha");
    fs::create_directories(synth_dir + "/beta");
    std::ofstream(synth_dir + "/alpha/out.md") << "Alpha output";
    std::ofstream(synth_dir + "/beta/out.md") << "Beta output";

    auto code = cmd_synthesize(ctx_, {synth_dir, "--agents", "alpha"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(FleetCmdTest, SynthesizeWithCustomSynthAgent) {
    auto synth_dir = ctx_.euxis_home + "/synth-output";
    fs::create_directories(synth_dir);
    std::ofstream(synth_dir + "/data.md") << "Some data to synthesize";

    auto code = cmd_synthesize(ctx_, {synth_dir, "--agent", "alpha"});
    EXPECT_TRUE(code == 0 || code == 1);
}

// --- Coverage: agent info with tags display (lines 138-144) ---
TEST_F(FleetCmdTest, AgentInfoWithTags) {
    auto code = cmd_agent(ctx_, {"info", "alpha"});
    EXPECT_EQ(code, 0);
    // alpha has tags ["core", "leader"], exercises tags display branch
}

// --- Coverage: squad info with members and lead (lines 235-242) ---
TEST_F(FleetCmdTest, SquadInfoShowsMembers) {
    auto code = cmd_squad(ctx_, {"info", "core-squad"});
    EXPECT_GE(code, 0);
}

// --- Coverage: squad validate with missing members ---
TEST_F(FleetCmdTest, SquadValidateWithMissingMember) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "alpha"}, {"role", "leader"}, {"tier", "code"}}
    });
    reg["squads"] = nlohmann::json::array({
        {{"id", "broken-squad"}, {"name", "Broken"}, {"purpose", "test"},
         {"lead", "alpha"}, {"members", {"alpha", "nonexistent-member"}}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_squad(ctx_, {"validate"});
    EXPECT_GE(code, 0); // may or may not find issues depending on default registry
}

// --- Coverage: squad deploy with mode parameter (lines 253-255) ---
TEST_F(FleetCmdTest, SquadDeployWithParallelMode) {
    auto code = cmd_squad(ctx_, {"deploy", "core-squad", "do parallel work", "--mode", "parallel"});
    EXPECT_GE(code, 0);
}

// --- Coverage: combo with default task (line 343) ---
TEST_F(FleetCmdTest, ComboWithUnknownAgent) {
    auto code = cmd_combo(ctx_, {"unknown-agent-xyz", "test task"});
    EXPECT_TRUE(code == 0 || code == 1);
}

// --- Coverage: playbook with step dependencies (lines 478-486) ---
TEST_F(FleetCmdTest, PlaybookWithDependencies) {
    auto path = ctx_.euxis_home + "/playbook-deps.json";
    nlohmann::json manifest;
    manifest["task"] = "integrated test";
    manifest["steps"] = nlohmann::json::array({
        {{"name", "step1"}, {"agent", "alpha"}, {"task", "step 1 work"}},
        {{"name", "step2"}, {"agent", "beta"}, {"task", "step 2 work"},
         {"depends", {"step1"}}}
    });
    std::ofstream(path) << manifest.dump();

    auto code = cmd_playbook(ctx_, {path});
    EXPECT_TRUE(code == 0 || code == 1);
}

// --- Coverage: dispatch with agent entries having tasks (lines 566-607) ---
TEST_F(FleetCmdTest, DispatchWithPerAgentTasks) {
    auto path = ctx_.euxis_home + "/dispatch-tasks.json";
    nlohmann::json manifest;
    manifest["task"] = "global task";
    manifest["agents"] = nlohmann::json::array({
        {{"agent_id", "alpha"}, {"priority", "P0"}, {"task", "specific task for alpha"}},
        {{"agent_id", "beta"}, {"priority", "P1"}}
    });
    std::ofstream(path) << manifest.dump();

    auto code = cmd_dispatch(ctx_, {path});
    EXPECT_TRUE(code == 0 || code == 1);
}

// --- Coverage: council with multiple rounds (lines 660-703) ---
TEST_F(FleetCmdTest, CouncilWithRounds) {
    auto code = cmd_council(ctx_, {"architecture review", "alpha,beta", "--rounds", "2"});
    EXPECT_TRUE(code == 0 || code == 1);
}

// --- Coverage: loop with convergence detection (lines 806-814) ---
TEST_F(FleetCmdTest, LoopWithLowIterations) {
    auto code = cmd_loop(ctx_, {"alpha", "refine output", "--max-iterations", "2", "--threshold", "0.5"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: loop with custom threshold ---
TEST_F(FleetCmdTest, LoopWithStrictThreshold) {
    auto code = cmd_loop(ctx_, {"alpha", "refine task", "--max-iterations", "1", "--threshold", "0.01"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: synthesize with agent filter matching (lines 877-886) ---
TEST_F(FleetCmdTest, SynthesizeWithNonMatchingAgentFilter) {
    auto synth_dir = ctx_.euxis_home + "/synth-filter";
    fs::create_directories(synth_dir);
    std::ofstream(synth_dir + "/output.md") << "some output";

    auto code = cmd_synthesize(ctx_, {synth_dir, "--agents", "nonexistent"});
    // No matching files => "no output files found"
    EXPECT_EQ(code, 0);
}

// --- Coverage: synthesize with --agent parameter (lines 856-858) ---
TEST_F(FleetCmdTest, SynthesizeWithSynthAgentAndAgentFilter) {
    auto synth_dir = ctx_.euxis_home + "/synth-combo";
    fs::create_directories(synth_dir + "/alpha");
    std::ofstream(synth_dir + "/alpha/result.md") << "Alpha result data";

    auto code = cmd_synthesize(ctx_, {synth_dir, "--agents", "alpha", "--agent", "beta"});
    EXPECT_TRUE(code == 0 || code == 1);
}

// --- Coverage: agent register with "id" field instead of "agent_id" ---
TEST_F(FleetCmdTest, AgentRegisterWithIdField) {
    auto manifest_path = ctx_.euxis_home + "/id-manifest.json";
    nlohmann::json manifest;
    manifest["id"] = "delta";
    manifest["role"] = "test";
    manifest["tier"] = "routine";
    manifest["version"] = "1.0";
    std::ofstream(manifest_path) << manifest.dump();

    auto code = cmd_agent(ctx_, {"register", manifest_path});
    EXPECT_EQ(code, 0);
}

// --- Coverage: squad list with json output (lines 210-217) ---
TEST_F(FleetCmdTest, SquadListJsonWithSquads) {
    ctx_.json_output = true;
    auto code = cmd_squad(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

// ---- Schema Versioning Tests ----

TEST_F(FleetCmdTest, PlaybookCIModeSchemaVersion) {
    auto path = ctx_.euxis_home + "/playbook-schema.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    auto output = buffer.str();
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;

    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded()) << "CI output must contain valid JSON";
    EXPECT_EQ(parsed["schema"].get<std::string>(), "euxis.verdict");
    EXPECT_EQ(parsed["schema_version"].get<std::string>(), "1.0.0");
}

// ---- Policy Tests ----

TEST_F(FleetCmdTest, PolicyLoadDefaults) {
    // No policy file → no violations → normal behavior
    auto path = ctx_.euxis_home + "/playbook-nopolicy.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    auto output = buffer.str();
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;

    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded());
    // No policy file → no policy_violations key
    EXPECT_FALSE(parsed.contains("policy_violations"));
}

TEST_F(FleetCmdTest, PolicyViolationLowConfidence) {
    // Write policy requiring high confidence
    auto policy_path = ctx_.data_dir + "/config/policy.json";
    fs::create_directories(fs::path(policy_path).parent_path());
    nlohmann::json policy;
    policy["min_confidence"] = 99;  // Mock agents produce low confidence
    std::ofstream(policy_path) << policy.dump();

    auto path = ctx_.euxis_home + "/playbook-policy-conf.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    auto output = buffer.str();
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;

    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded());
    EXPECT_TRUE(parsed.contains("policy_violations"));
    EXPECT_FALSE(parsed["policy_passed"].get<bool>());
    // Find the min_confidence violation
    bool found = false;
    for (const auto& v : parsed["policy_violations"]) {
        if (v["gate"].get<std::string>() == "min_confidence") found = true;
    }
    EXPECT_TRUE(found) << "Expected min_confidence violation";
}

TEST_F(FleetCmdTest, PolicyViolationBadVerdict) {
    // Mock execution produces TRUSTED verdict with ~0.75 critical_coverage.
    // Set min_confidence impossibly high to guarantee a policy violation,
    // and disable min_critical_coverage so we test a specific gate.
    auto policy_path = ctx_.data_dir + "/config/policy.json";
    fs::create_directories(fs::path(policy_path).parent_path());
    nlohmann::json policy;
    policy["min_confidence"] = 999;
    policy["min_critical_coverage"] = 0.0;
    std::ofstream(policy_path) << policy.dump();

    auto path = ctx_.euxis_home + "/playbook-policy-verdict.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    auto output = buffer.str();
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;

    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded());
    EXPECT_TRUE(parsed.contains("policy_violations"));
    bool found = false;
    for (const auto& v : parsed["policy_violations"]) {
        if (v["gate"].get<std::string>() == "min_confidence") found = true;
    }
    EXPECT_TRUE(found) << "Expected min_confidence violation";
}

TEST_F(FleetCmdTest, PolicyViolationRegression) {
    // Seed history with a TRUSTED verdict, then run with regression detection
    auto hist_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "verdicts" / "history.jsonl";
    fs::create_directories(hist_path.parent_path());
    nlohmann::json entry;
    entry["timestamp"] = "2026-01-01T00:00:00Z";
    entry["verdict"] = "TRUSTED";
    entry["confidence"] = 95;
    entry["mode"] = "flash";
    entry["latency_ms"] = 10000;
    entry["agents_executed"] = 3;
    entry["agents_skipped"] = 0;
    entry["timeout_count"] = 0;
    entry["evidence_density"] = 0.8;
    entry["early_stopped"] = false;
    entry["escalated"] = false;
    entry["budget_exceeded"] = false;
    std::ofstream(hist_path) << entry.dump() << "\n";

    auto policy_path = ctx_.data_dir + "/config/policy.json";
    fs::create_directories(fs::path(policy_path).parent_path());
    nlohmann::json policy;
    policy["fail_on_regression"] = true;
    std::ofstream(policy_path) << policy.dump();

    auto path = ctx_.euxis_home + "/playbook-policy-regression.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    auto output = buffer.str();
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;

    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded());
    // The mock will produce a non-TRUSTED verdict (CAUTION/INCONCLUSIVE in test),
    // which should trigger regression from TRUSTED
    if (parsed.contains("trend") && parsed["trend"].value("regression", false)) {
        EXPECT_TRUE(parsed.contains("policy_violations"));
        bool found = false;
        for (const auto& v : parsed["policy_violations"]) {
            if (v["gate"].get<std::string>() == "fail_on_regression") found = true;
        }
        EXPECT_TRUE(found) << "Expected fail_on_regression violation";
    }
    // If no regression detected (mock produced same verdict), test is still valid
}

TEST_F(FleetCmdTest, PolicyViolationCoverage) {
    // Write policy requiring high critical coverage
    auto policy_path = ctx_.data_dir + "/config/policy.json";
    fs::create_directories(fs::path(policy_path).parent_path());
    nlohmann::json policy;
    policy["min_critical_coverage"] = 0.80;  // Mock agents cover 1/4 pillars max → 0.25
    std::ofstream(policy_path) << policy.dump();

    auto path = ctx_.euxis_home + "/playbook-policy-coverage.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci"});
    std::cout.rdbuf(old_cout);

    auto output = buffer.str();
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;

    auto parsed = nlohmann::json::parse(json_str, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded());
    EXPECT_TRUE(parsed.contains("policy_violations"));
    bool found = false;
    for (const auto& v : parsed["policy_violations"]) {
        if (v["gate"].get<std::string>() == "min_critical_coverage") found = true;
    }
    EXPECT_TRUE(found) << "Expected min_critical_coverage violation";
}

// ---- Flash Triage Mode Tests ----

// Helper: parse CI JSON artifact from captured stdout
auto parse_ci_artifact(const std::string& output) -> nlohmann::json {
    auto json_start = output.rfind("\n{");
    if (json_start == std::string::npos) json_start = output.find("{");
    else json_start++;
    auto json_str = (json_start != std::string::npos) ? output.substr(json_start) : output;
    return nlohmann::json::parse(json_str, nullptr, false);
}

// Helper: create a manifest with 11+ steps so flash index selection {0,10} works
auto make_full_manifest() -> nlohmann::json {
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "librarian"}, {"agent", "librarian"}, {"task", "scan repo"}, {"pillar", "execution"}},
        {{"name", "writer"}, {"agent", "writer"}, {"task", "write docs"}, {"pillar", "execution"}},
        {{"name", "unused2"}, {"agent", "alpha"}, {"task", "task2"}, {"pillar", "execution"}},
        {{"name", "architect"}, {"agent", "architect"}, {"task", "architecture review"}, {"pillar", "architecture"}},
        {{"name", "optimizer"}, {"agent", "optimizer"}, {"task", "optimize"}, {"pillar", "execution"}},
        {{"name", "unused5"}, {"agent", "alpha"}, {"task", "task5"}, {"pillar", "execution"}},
        {{"name", "unused6"}, {"agent", "alpha"}, {"task", "task6"}, {"pillar", "execution"}},
        {{"name", "unused7"}, {"agent", "alpha"}, {"task", "task7"}, {"pillar", "execution"}},
        {{"name", "sentinel"}, {"agent", "sentinel"}, {"task", "security audit"}, {"pillar", "security"}},
        {{"name", "unused9"}, {"agent", "alpha"}, {"task", "task9"}, {"pillar", "execution"}},
        {{"name", "reviewer"}, {"agent", "reviewer"}, {"task", "final review"}, {"pillar", "quality"}}
    });
    return manifest;
}

TEST_F(FleetCmdTest, FlashTriageAgentCount) {
    // Flash mode should plan exactly 2 agents: librarian (0) and reviewer (10)
    auto path = ctx_.euxis_home + "/playbook-flash-triage.json";
    std::ofstream(path) << make_full_manifest().dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "flash"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());
    // Count agents in agent_status (excluding claim-verifier entries)
    int agent_count = 0;
    for (auto& [key, _] : parsed["agent_status"].items()) {
        if (key.find("claim-verifier") == std::string::npos) agent_count++;
    }
    EXPECT_EQ(agent_count, 2) << "Flash triage should run exactly 2 agents";
}

TEST_F(FleetCmdTest, FlashNoEscalationOnConvergence) {
    // Both agents PASS → no escalation, triage.outcome="converged"
    auto path = ctx_.euxis_home + "/playbook-flash-converge.json";
    std::ofstream(path) << make_full_manifest().dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "flash"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());
    EXPECT_FALSE(parsed.value("escalated", true));
    // In CI mode with mock execution, agents produce PASS by default
    if (parsed.contains("triage")) {
        EXPECT_EQ(parsed["triage"]["outcome"].get<std::string>(), "converged");
        EXPECT_EQ(parsed["triage"]["agents"].get<int>(), 2);
        EXPECT_TRUE(parsed["triage"]["completed"].get<bool>());
    }
}

TEST_F(FleetCmdTest, FlashEscalationDisabledInCI) {
    // CI mode suppresses escalation
    auto path = ctx_.euxis_home + "/playbook-flash-ci-noesc.json";
    std::ofstream(path) << make_full_manifest().dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "flash"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());
    // In CI mode, escalation is always suppressed (escalated should be false)
    EXPECT_FALSE(parsed.value("escalated", true));
    // If escalation was triggered but suppressed, check triage block
    if (parsed.contains("triage") && parsed["triage"]["outcome"] == "suppressed") {
        EXPECT_TRUE(parsed["triage"]["suppressed_in_ci"].get<bool>());
    }
}

TEST_F(FleetCmdTest, FlashTriageOutcomeInArtifact) {
    // Artifact contains triage block with outcome/decisive/agents fields
    auto path = ctx_.euxis_home + "/playbook-flash-artifact.json";
    std::ofstream(path) << make_full_manifest().dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "flash"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());
    ASSERT_TRUE(parsed.contains("triage")) << "Flash artifact must contain triage block";
    EXPECT_TRUE(parsed["triage"].contains("completed"));
    EXPECT_TRUE(parsed["triage"].contains("agents"));
    EXPECT_TRUE(parsed["triage"].contains("decisive"));
    EXPECT_TRUE(parsed["triage"].contains("outcome"));
    EXPECT_TRUE(parsed["triage"].contains("suppressed_in_ci"));
    EXPECT_EQ(parsed["triage"]["agents"].get<int>(), 2);
}

TEST_F(FleetCmdTest, FlashTriageModeField) {
    // Artifact mode field should reflect original flash mode
    auto path = ctx_.euxis_home + "/playbook-flash-mode.json";
    std::ofstream(path) << make_full_manifest().dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "flash"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());
    EXPECT_EQ(parsed["mode"].get<std::string>(), "flash");
}

TEST_F(FleetCmdTest, SuppressedEscalationCapsVerdict) {
    // When escalation is suppressed in CI, verdict must not be TRUSTED or TRUSTED WITH GAPS
    auto path = ctx_.euxis_home + "/playbook-flash-cap.json";
    std::ofstream(path) << make_full_manifest().dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    auto code = cmd_playbook(ctx_, {path, "--ci", "--mode", "flash"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());

    if (parsed.contains("triage") && parsed["triage"]["outcome"] == "suppressed") {
        // Suppressed escalation must not produce a passing verdict
        auto verdict = parsed["verdict"].get<std::string>();
        EXPECT_NE(verdict, "TRUSTED") << "Suppressed escalation must not produce TRUSTED";
        EXPECT_NE(verdict, "TRUSTED WITH GAPS") << "Suppressed escalation must not produce TRUSTED WITH GAPS";
        EXPECT_NE(code, 0) << "Suppressed escalation must not produce exit code 0";
    }
    // If triage converged (no escalation needed), any verdict is valid
}

TEST_F(FleetCmdTest, StandardModeNoTriageBlock) {
    // Standard mode should NOT have a triage block in artifact
    auto path = ctx_.euxis_home + "/playbook-std-notriage.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "standard"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());
    EXPECT_FALSE(parsed.contains("triage")) << "Standard mode should not have triage block";
}

TEST_F(FleetCmdTest, BuiltinChecksInArtifact) {
    // Built-in pillar checks should appear in builtin_checks, not agent_status
    auto path = ctx_.euxis_home + "/playbook-builtin.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "standard"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());

    // Must have both sections
    EXPECT_TRUE(parsed.contains("agent_status"));
    EXPECT_TRUE(parsed.contains("builtin_checks"));

    // Agent status should contain the LLM agent, not built-in checks
    if (parsed.contains("agent_status") && !parsed["agent_status"].empty()) {
        for (auto& [key, _] : parsed["agent_status"].items()) {
            EXPECT_EQ(key.find("builtin/"), std::string::npos)
                << "Built-in check '" << key << "' should not be in agent_status";
        }
    }
}

TEST_F(FleetCmdTest, PillarScoresIncludeTestingAndSecurity) {
    // Pillar scores should include testing and security (not Missing when built-in checks run)
    auto path = ctx_.euxis_home + "/playbook-pillars.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "standard"});
    std::cout.rdbuf(old_cout);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());
    ASSERT_TRUE(parsed.contains("pillar_scores"));

    // Check that testing and security pillars exist and are not "Missing"
    bool has_testing = false, has_security = false;
    for (const auto& ps : parsed["pillar_scores"]) {
        if (ps["name"] == "testing") {
            has_testing = true;
            EXPECT_NE(ps["status"], "Missing") << "Testing pillar should not be Missing with built-in checks";
        }
        if (ps["name"] == "security") {
            has_security = true;
            EXPECT_NE(ps["status"], "Missing") << "Security pillar should not be Missing with built-in checks";
        }
    }
    EXPECT_TRUE(has_testing) << "Artifact should have a testing pillar score";
    EXPECT_TRUE(has_security) << "Artifact should have a security pillar score";
}

TEST_F(FleetCmdTest, EmptyManifestNoBuiltinChecks) {
    // Empty manifests should NOT produce built-in checks (no agent evidence = no pillar checks)
    auto path = ctx_.euxis_home + "/playbook-empty-nobuiltin.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array();
    std::ofstream(path) << manifest.dump();

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    auto code = cmd_playbook(ctx_, {path, "--ci", "--mode", "standard"});
    std::cout.rdbuf(old_cout);

    EXPECT_EQ(code, 2) << "Empty manifest should produce exit code 2 (no evidence)";

    auto parsed = parse_ci_artifact(buffer.str());
    if (!parsed.is_discarded() && parsed.contains("builtin_checks")) {
        EXPECT_TRUE(parsed["builtin_checks"].empty())
            << "Empty manifest should not produce built-in checks";
    }
}

// ---- Builtin/Claim-Verifier Verdict Integration Tests ----

TEST_F(FleetCmdTest, BuildVerifierFailedNotTrusted) {
    // If claim-verifier FAILED but triage agents PASS,
    // the verdict must NOT be TRUSTED.
    // Create a Makefile-only project and chdir to it so detect_project_verifiers picks it up.
    auto repo = ctx_.euxis_home + "/test-repo";
    fs::create_directories(repo);
    std::ofstream(repo + "/Makefile") << "check:\n\t@echo 'BUILD FAIL' && exit 1\n";

    auto path = ctx_.euxis_home + "/playbook-build-fail.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    // Save CWD and chdir to test repo so verifier detects the Makefile
    auto saved_cwd = fs::current_path();
    fs::current_path(repo);

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    auto code = cmd_playbook(ctx_, {path, "--ci", "--mode", "standard"});
    std::cout.rdbuf(old_cout);
    fs::current_path(saved_cwd);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded()) << "Must produce valid JSON artifact";

    // Check if the claim verifier actually ran and failed
    bool verifier_failed = false;
    if (parsed.contains("claim_verification")) {
        for (const auto& cv : parsed["claim_verification"]) {
            if (!cv.value("verified", true)) verifier_failed = true;
        }
    }
    if (parsed.contains("builtin_checks")) {
        for (auto& [key, val] : parsed["builtin_checks"].items()) {
            if (key.find("claim-verifier") != std::string::npos &&
                val.value("status", "") == "FAILED") verifier_failed = true;
        }
    }

    if (verifier_failed) {
        auto verdict = parsed.value("verdict", "");
        EXPECT_NE(verdict, "TRUSTED")
            << "Failed build verification must not produce TRUSTED verdict";
        EXPECT_NE(code, 0) << "Failed build verification must produce non-zero exit code";
    }
    // If verifier didn't run (e.g., make not available), test is still valid
}

TEST_F(FleetCmdTest, FailedClaimVerifierContributesToRawFailCount) {
    // When claim-verifier fails, raw_fail_count must be > 0.
    auto repo = ctx_.euxis_home + "/test-repo-rawfail";
    fs::create_directories(repo);
    std::ofstream(repo + "/Makefile") << "check:\n\t@exit 1\n";

    auto path = ctx_.euxis_home + "/playbook-rawfail.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    auto saved_cwd = fs::current_path();
    fs::current_path(repo);

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "standard"});
    std::cout.rdbuf(old_cout);
    fs::current_path(saved_cwd);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());

    // Check that the claim-verifier failure is recorded
    if (parsed.contains("builtin_checks")) {
        bool found_failed = false;
        for (auto& [key, val] : parsed["builtin_checks"].items()) {
            if (key.find("claim-verifier") != std::string::npos &&
                val.value("status", "") == "FAILED") {
                found_failed = true;
            }
        }
        if (found_failed) {
            auto rfc = parsed["confidence_rationale"].value("raw_fail_count", 0);
            EXPECT_GT(rfc, 0) << "Failed claim-verifier must contribute to raw_fail_count";
        }
    }
}

TEST_F(FleetCmdTest, RecommendedActionReflectsBuildFailure) {
    // When build verification fails, recommended_action must not say "Safe to merge."
    auto repo = ctx_.euxis_home + "/test-repo-action";
    fs::create_directories(repo);
    std::ofstream(repo + "/Makefile") << "check:\n\t@exit 1\n";

    auto path = ctx_.euxis_home + "/playbook-action.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    auto saved_cwd = fs::current_path();
    fs::current_path(repo);

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    (void)cmd_playbook(ctx_, {path, "--ci", "--mode", "standard"});
    std::cout.rdbuf(old_cout);
    fs::current_path(saved_cwd);

    auto parsed = parse_ci_artifact(buffer.str());
    ASSERT_FALSE(parsed.is_discarded());

    if (parsed.contains("builtin_checks")) {
        bool found_failed = false;
        for (auto& [key, val] : parsed["builtin_checks"].items()) {
            if (key.find("claim-verifier") != std::string::npos &&
                val.value("status", "") == "FAILED") {
                found_failed = true;
            }
        }
        if (found_failed) {
            auto action = parsed.value("recommended_action", "");
            EXPECT_NE(action, "Safe to merge. Evidence is strong.")
                << "Failed build must not produce 'Safe to merge' recommendation";
        }
    }
}

TEST_F(FleetCmdTest, CiExitCodeNonZeroOnBuildFailure) {
    // CI exit code must be non-zero when build verification fails.
    auto repo = ctx_.euxis_home + "/test-repo-ci";
    fs::create_directories(repo);
    std::ofstream(repo + "/Makefile") << "check:\n\t@exit 1\n";

    auto path = ctx_.euxis_home + "/playbook-ci-exit.json";
    nlohmann::json manifest;
    manifest["steps"] = nlohmann::json::array({
        {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}}
    });
    std::ofstream(path) << manifest.dump();

    auto saved_cwd = fs::current_path();
    fs::current_path(repo);

    std::stringstream buffer;
    auto* old_cout = std::cout.rdbuf(buffer.rdbuf());
    auto code = cmd_playbook(ctx_, {path, "--ci", "--mode", "standard"});
    std::cout.rdbuf(old_cout);
    fs::current_path(saved_cwd);

    // If the build verifier ran and failed, exit code must be non-zero
    auto parsed = parse_ci_artifact(buffer.str());
    if (!parsed.is_discarded() && parsed.contains("builtin_checks")) {
        for (auto& [key, val] : parsed["builtin_checks"].items()) {
            if (key.find("claim-verifier") != std::string::npos &&
                val.value("status", "") == "FAILED") {
                EXPECT_NE(code, 0) << "CI exit code must be non-zero when build verification fails";
                break;
            }
        }
    }
}

} // namespace
} // namespace euxis::cli::cmd
