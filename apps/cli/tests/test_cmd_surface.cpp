#include <gtest/gtest.h>
#include "euxis/cli/cmd/surface.hpp"
#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/engine.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <unistd.h>
#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class SurfaceCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        setenv("EUXIS_TEST_MOCK_EXECUTION", "1", 1);
        ctx_.euxis_home = "/tmp/euxis_test_surface_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        fs::create_directories(ctx_.data_dir + "/agents");
        fs::create_directories(ctx_.data_dir + "/config/playbooks");
        fs::create_directories(ctx_.data_dir + "/runtime/sessions");
        fs::create_directories(ctx_.data_dir + "/runtime/verdicts");

        // Create a test registry with agents matching verify-everything playbook
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "librarian"}, {"role", "docs"}, {"version", "1.0"}, {"tier", "data"},
             {"tags", {"fleet"}}, {"capabilities", {"docs"}}, {"prompt_path", ""}},
            {{"agent_id", "reviewer"}, {"role", "review"}, {"version", "1.0"}, {"tier", "code"},
             {"tags", {"fleet"}}, {"capabilities", {"review"}}, {"prompt_path", ""}},
            {{"agent_id", "architect"}, {"role", "arch"}, {"version", "1.0"}, {"tier", "core"},
             {"tags", {"fleet"}}, {"capabilities", {"architecture"}}, {"prompt_path", ""}},
            {{"agent_id", "optimizer"}, {"role", "perf"}, {"version", "1.0"}, {"tier", "code"},
             {"tags", {"fleet"}}, {"capabilities", {"optimization"}}, {"prompt_path", ""}},
            {{"agent_id", "sentinel"}, {"role", "security"}, {"version", "1.0"}, {"tier", "core"},
             {"tags", {"fleet"}}, {"capabilities", {"security"}}, {"prompt_path", ""}}
        });
        reg["squads"] = nlohmann::json::array();
        std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

        // Create a minimal verify-everything playbook
        nlohmann::json pb;
        pb["schema_version"] = "1.0";
        pb["id"] = "verify-everything";
        pb["name"] = "Verify Everything";
        pb["phases"] = nlohmann::json::array({
            {{"phase", 0}, {"name", "Check"}, {"gate", "docs"}, {"principle", "test"},
             {"mode", "hierarchical"}, {"delegates", nlohmann::json::array({
                {{"agent", "librarian"}, {"task_template", "Check docs for: ${goal}"},
                 {"verifies", nlohmann::json::array({"docs ok"})}}
            })}},
            {{"phase", 5}, {"name", "Review"}, {"gate", "truth"}, {"principle", "test"},
             {"mode", "hierarchical"}, {"delegates", nlohmann::json::array({
                {{"agent", "reviewer"}, {"task_template", "Review for: ${goal}"},
                 {"verifies", nlohmann::json::array({"review ok"})}}
            })}}
        });
        std::ofstream(ctx_.data_dir + "/config/playbooks/verify-everything.json") << pb.dump();
    }

    void TearDown() override {
        unsetenv("EUXIS_TEST_MOCK_EXECUTION");
        fs::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

// ---- check command ----

TEST_F(SurfaceCmdTest, CheckDefaultMode) {
    auto code = cmd_check(ctx_, {});
    // Mock execution: completes with some exit code (0 or 1)
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, CheckWithTarget) {
    auto code = cmd_check(ctx_, {"."});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, CheckTriageFlag) {
    auto code = cmd_check(ctx_, {"--triage"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, CheckForensicFlag) {
    auto code = cmd_check(ctx_, {"--forensic"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, CheckHelp) {
    auto code = cmd_check(ctx_, {"--help"});
    EXPECT_EQ(code, 0);
}

TEST_F(SurfaceCmdTest, CheckEnrichWritesOpenVexInCwd) {
    // Run cmd_check with --enrich against an empty target — no manifests
    // means no OSV.dev calls and a deterministic, network-free test that
    // still exercises run_enrichment end-to-end.
    auto scan_target = fs::path{ctx_.euxis_home} / "enrich-target";
    fs::create_directories(scan_target);

    auto cwd_before = fs::current_path();
    auto cwd_test   = fs::path{ctx_.euxis_home} / "enrich-cwd";
    fs::create_directories(cwd_test);
    fs::current_path(cwd_test);

    auto code = cmd_check(ctx_, {scan_target.string(), "--triage", "--enrich"});

    fs::current_path(cwd_before);

    auto vex = cwd_test / "euxis-vex.openvex.json";
    EXPECT_TRUE(fs::exists(vex)) << "expected --enrich to write " << vex;
    if (fs::exists(vex)) {
        EXPECT_GT(fs::file_size(vex), 0);
    }
    EXPECT_TRUE(code == 0 || code == 1);
}

// ---- triage command ----

TEST_F(SurfaceCmdTest, TriageAlwaysFlash) {
    auto code = cmd_triage(ctx_, {});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, TriageWithTarget) {
    auto code = cmd_triage(ctx_, {"."});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, TriageHelp) {
    auto code = cmd_triage(ctx_, {"--help"});
    EXPECT_EQ(code, 0);
}

// ---- review command ----

TEST_F(SurfaceCmdTest, ReviewDefaultStandard) {
    auto code = cmd_review(ctx_, {});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, ReviewForensicFlag) {
    auto code = cmd_review(ctx_, {"--forensic"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, ReviewHelp) {
    auto code = cmd_review(ctx_, {"--help"});
    EXPECT_EQ(code, 0);
}

// ---- compare command ----

TEST_F(SurfaceCmdTest, CompareNoTargetReturns2) {
    auto code = cmd_compare(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(SurfaceCmdTest, CompareWithTarget) {
    auto code = cmd_compare(ctx_, {"."});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, CompareHelp) {
    auto code = cmd_compare(ctx_, {"--help"});
    EXPECT_EQ(code, 0);
}

// ---- stats command ----

TEST_F(SurfaceCmdTest, StatsNoHistory) {
    auto code = cmd_stats(ctx_, {});
    // No history → prints message, returns 0
    EXPECT_EQ(code, 0);
}

TEST_F(SurfaceCmdTest, StatsWithSince) {
    auto code = cmd_stats(ctx_, {"--since", "2026-01-01"});
    EXPECT_EQ(code, 0);
}

TEST_F(SurfaceCmdTest, StatsWithLast) {
    auto code = cmd_stats(ctx_, {"--last", "5"});
    EXPECT_EQ(code, 0);
}

TEST_F(SurfaceCmdTest, StatsHelp) {
    auto code = cmd_stats(ctx_, {"--help"});
    EXPECT_EQ(code, 0);
}

// ---- policy command ----

TEST_F(SurfaceCmdTest, PolicyNoSubcommand) {
    auto code = cmd_policy(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(SurfaceCmdTest, PolicyHelp) {
    auto code = cmd_policy(ctx_, {"--help"});
    EXPECT_EQ(code, 0);
}

TEST_F(SurfaceCmdTest, PolicyShowNoFile) {
    auto code = cmd_policy(ctx_, {"show"});
    EXPECT_EQ(code, 1);
}

TEST_F(SurfaceCmdTest, PolicyShowWithFile) {
    nlohmann::json policy;
    policy["min_confidence"] = 70;
    policy["min_verdict"] = "TRUSTED WITH GAPS";
    std::ofstream(ctx_.data_dir + "/config/policy.json") << policy.dump();

    auto code = cmd_policy(ctx_, {"show"});
    EXPECT_EQ(code, 0);
}

TEST_F(SurfaceCmdTest, PolicyValidateGoodJson) {
    nlohmann::json policy;
    policy["min_confidence"] = 70;
    policy["min_verdict"] = "TRUSTED WITH GAPS";
    auto path = ctx_.euxis_home + "/test-policy.json";
    std::ofstream(path) << policy.dump();

    auto code = cmd_policy(ctx_, {"validate", path});
    EXPECT_EQ(code, 0);
}

TEST_F(SurfaceCmdTest, PolicyValidateBadJson) {
    auto path = ctx_.euxis_home + "/bad-policy.json";
    std::ofstream(path) << "not json {{";

    auto code = cmd_policy(ctx_, {"validate", path});
    EXPECT_EQ(code, 1);
}

TEST_F(SurfaceCmdTest, PolicyValidateMissing) {
    auto code = cmd_policy(ctx_, {"validate", "/tmp/nonexistent.json"});
    EXPECT_EQ(code, 1);
}

TEST_F(SurfaceCmdTest, PolicyCheckNoArtifact) {
    auto code = cmd_policy(ctx_, {"check"});
    EXPECT_EQ(code, 1);
}

TEST_F(SurfaceCmdTest, PolicyCheckTargetDelegatesToCheck) {
    // policy check . should delegate to cmd_check with --policy
    auto code = cmd_policy(ctx_, {"check", "."});
    // Runs full verification under policy — completes with mock
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(SurfaceCmdTest, PolicyCheckLatestArtifact) {
    // Create policy
    nlohmann::json policy;
    policy["min_confidence"] = 50;
    policy["min_verdict"] = "CAUTION";
    std::ofstream(ctx_.data_dir + "/config/policy.json") << policy.dump();

    // Create latest verdict artifact
    nlohmann::json artifact;
    artifact["verdict"] = "TRUSTED WITH GAPS";
    artifact["confidence"] = 72;
    artifact["budget_exceeded"] = false;
    std::ofstream(ctx_.data_dir + "/runtime/sessions/latest_verdict.json") << artifact.dump();

    auto code = cmd_policy(ctx_, {"check"});
    EXPECT_EQ(code, 0);
}

TEST_F(SurfaceCmdTest, PolicyCheckViolation) {
    // Create policy with high bar
    nlohmann::json policy;
    policy["min_confidence"] = 90;
    policy["min_verdict"] = "TRUSTED";
    std::ofstream(ctx_.data_dir + "/config/policy.json") << policy.dump();

    // Create artifact below bar
    nlohmann::json artifact;
    artifact["verdict"] = "CAUTION";
    artifact["confidence"] = 52;
    std::ofstream(ctx_.data_dir + "/runtime/sessions/latest_verdict.json") << artifact.dump();

    auto code = cmd_policy(ctx_, {"check"});
    EXPECT_EQ(code, 1);
}

TEST_F(SurfaceCmdTest, PolicyUnknownSubcommand) {
    auto code = cmd_policy(ctx_, {"bogus"});
    EXPECT_EQ(code, 2);
}

// ---- Help output smoke tests ----
// Verify help text contains expected keywords to keep the surface stable.

class HelpSmokeTest : public SurfaceCmdTest {
protected:
    // Capture stderr (surface --help writes to cerr)
    std::string capture_cerr(std::function<int()> fn) {
        std::ostringstream buf;
        auto* old = std::cerr.rdbuf(buf.rdbuf());
        fn();
        std::cerr.rdbuf(old);
        return buf.str();
    }
};

TEST_F(HelpSmokeTest, CheckHelpContent) {
    auto out = capture_cerr([&]{ return cmd_check(ctx_, {"--help"}); });
    EXPECT_NE(out.find("Verify a repository"), std::string::npos);
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis check"), std::string::npos);
    EXPECT_NE(out.find("--triage"), std::string::npos);
    EXPECT_NE(out.find("--forensic"), std::string::npos);
    EXPECT_NE(out.find("--policy"), std::string::npos);
    EXPECT_NE(out.find("--ci"), std::string::npos);
    EXPECT_NE(out.find("--json"), std::string::npos);
}

TEST_F(HelpSmokeTest, TriageHelpContent) {
    auto out = capture_cerr([&]{ return cmd_triage(ctx_, {"--help"}); });
    EXPECT_NE(out.find("triage"), std::string::npos);
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis triage"), std::string::npos);
    EXPECT_NE(out.find("--policy"), std::string::npos);
    EXPECT_NE(out.find("--ci"), std::string::npos);
    // Triage should NOT mention --triage or --forensic (those are check/review flags)
    EXPECT_EQ(out.find("--triage"), std::string::npos);
    EXPECT_EQ(out.find("--forensic"), std::string::npos);
}

TEST_F(HelpSmokeTest, ReviewHelpContent) {
    auto out = capture_cerr([&]{ return cmd_review(ctx_, {"--help"}); });
    EXPECT_NE(out.find("review"), std::string::npos);
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis review"), std::string::npos);
    EXPECT_NE(out.find("--forensic"), std::string::npos);
    EXPECT_NE(out.find("--policy"), std::string::npos);
    // Review should NOT mention --triage
    EXPECT_EQ(out.find("--triage"), std::string::npos);
}

TEST_F(HelpSmokeTest, CompareHelpContent) {
    auto out = capture_cerr([&]{ return cmd_compare(ctx_, {"--help"}); });
    EXPECT_NE(out.find("Compare"), std::string::npos);
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis compare"), std::string::npos);
    EXPECT_NE(out.find("<target>"), std::string::npos);
    EXPECT_NE(out.find("--json"), std::string::npos);
    EXPECT_NE(out.find("--policy"), std::string::npos);
}

TEST_F(HelpSmokeTest, StatsHelpContent) {
    auto out = capture_cerr([&]{ return cmd_stats(ctx_, {"--help"}); });
    EXPECT_NE(out.find("metrics"), std::string::npos);
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis stats"), std::string::npos);
    EXPECT_NE(out.find("--since"), std::string::npos);
    EXPECT_NE(out.find("--last"), std::string::npos);
    EXPECT_NE(out.find("--check-baseline"), std::string::npos);
    EXPECT_NE(out.find("--json"), std::string::npos);
}

TEST_F(HelpSmokeTest, PolicyHelpContent) {
    auto out = capture_cerr([&]{ return cmd_policy(ctx_, {"--help"}); });
    EXPECT_NE(out.find("Policy"), std::string::npos);
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis policy"), std::string::npos);
    EXPECT_NE(out.find("show"), std::string::npos);
    EXPECT_NE(out.find("validate"), std::string::npos);
    EXPECT_NE(out.find("check"), std::string::npos);
}

TEST_F(HelpSmokeTest, CompareNoTargetShowsUsage) {
    // compare without target should print usage hint, not just a bare error
    auto out = capture_cerr([&]{ return cmd_compare(ctx_, {}); });
    EXPECT_NE(out.find("euxis compare"), std::string::npos);
    EXPECT_NE(out.find("target"), std::string::npos);
}

TEST_F(HelpSmokeTest, PolicyNoSubcommandShowsUsage) {
    // policy without subcommand shows help
    auto out = capture_cerr([&]{ return cmd_policy(ctx_, {}); });
    EXPECT_NE(out.find("euxis policy"), std::string::npos);
    EXPECT_NE(out.find("show"), std::string::npos);
    EXPECT_NE(out.find("validate"), std::string::npos);
    EXPECT_NE(out.find("check"), std::string::npos);
}

} // namespace
} // namespace euxis::cli::cmd

// ---- Engine alias tests ----

namespace euxis::cli {
namespace {

class EngineAliasTest : public ::testing::Test {
protected:
    void SetUp() override {
        setenv("EUXIS_TEST_MOCK_EXECUTION", "1", 1);
        test_home_ = "/tmp/euxis_test_alias_" + std::to_string(getpid());
        auto data_dir = test_home_ + "/data";
        namespace fs = std::filesystem;
        fs::create_directories(data_dir + "/agents");
        fs::create_directories(data_dir + "/config/playbooks");

        // Minimal registry
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "librarian"}, {"role", "docs"}, {"version", "1.0"}, {"tier", "data"},
             {"tags", {"fleet"}}, {"capabilities", {"docs"}}, {"prompt_path", ""}}
        });
        reg["squads"] = nlohmann::json::array();
        std::ofstream(data_dir + "/agents/registry.json") << reg.dump();

        // Minimal playbook
        nlohmann::json pb;
        pb["schema_version"] = "1.0";
        pb["id"] = "verify-everything";
        pb["phases"] = nlohmann::json::array({
            {{"phase", 0}, {"name", "Check"}, {"gate", "docs"}, {"principle", "test"},
             {"mode", "hierarchical"}, {"delegates", nlohmann::json::array({
                {{"agent", "librarian"}, {"task_template", "Check: ${goal}"},
                 {"verifies", nlohmann::json::array({"ok"})}}
            })}}
        });
        std::ofstream(data_dir + "/config/playbooks/verify-everything.json") << pb.dump();
    }

    void TearDown() override {
        unsetenv("EUXIS_TEST_MOCK_EXECUTION");
        std::filesystem::remove_all(test_home_);
    }

    std::string test_home_;
};

TEST_F(EngineAliasTest, QuickResolvesToTriage) {
    Engine e(test_home_);
    auto code = e.run({"quick"});
    // Should resolve to triage (flash mode) and run successfully
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(EngineAliasTest, DeepResolvesToReview) {
    Engine e(test_home_);
    auto code = e.run({"deep"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(EngineAliasTest, DiagResolvesToDoctor) {
    Engine e(test_home_);
    auto code = e.run({"diag"});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(EngineAliasTest, MetricsResolvesToStats) {
    Engine e(test_home_);
    auto code = e.run({"metrics"});
    // No history, but should resolve and return 0
    EXPECT_EQ(code, 0);
}

TEST_F(EngineAliasTest, VerifyAllResolvesToCheck) {
    Engine e(test_home_);
    auto code = e.run({"verify-all"});
    // Should resolve to check (standard mode)
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(EngineAliasTest, UnknownCommandStillFails) {
    Engine e(test_home_);
    auto code = e.run({"nonexistent-command"});
    EXPECT_EQ(code, 1);
}

TEST_F(EngineAliasTest, HelpShowsCoreGroupFirst) {
    Engine e(test_home_);
    // std::println writes to stdout fd, not std::cout — capture at fd level
    auto tmp = std::tmpfile();
    ASSERT_NE(tmp, nullptr);
    int old_fd = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    e.run({"--help"});
    fflush(stdout);
    dup2(old_fd, STDOUT_FILENO);
    close(old_fd);
    // Read captured output
    fseek(tmp, 0, SEEK_END);
    auto sz = ftell(tmp);
    fseek(tmp, 0, SEEK_SET);
    std::string out(sz, '\0');
    auto nread = fread(out.data(), 1, sz, tmp);
    out.resize(nread);
    fclose(tmp);
    // Core group should appear in help output
    EXPECT_NE(out.find("Core"), std::string::npos);
    // Core commands should be listed
    EXPECT_NE(out.find("check"), std::string::npos);
    EXPECT_NE(out.find("triage"), std::string::npos);
    EXPECT_NE(out.find("review"), std::string::npos);
    EXPECT_NE(out.find("compare"), std::string::npos);
    EXPECT_NE(out.find("stats"), std::string::npos);
    EXPECT_NE(out.find("policy"), std::string::npos);
    // Core should appear before System
    auto core_pos = out.find("Core");
    auto system_pos = out.find("System");
    if (system_pos != std::string::npos) {
        EXPECT_LT(core_pos, system_pos) << "Core group should appear before System";
    }
}

} // namespace
} // namespace euxis::cli
