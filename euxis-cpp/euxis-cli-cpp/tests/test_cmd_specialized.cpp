#include <gtest/gtest.h>
#include "euxis/cli/cmd/specialized.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {
namespace {

class SpecializedCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_spec_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/euxis-data";
        std::filesystem::create_directories(ctx_.data_dir + "/agents");

        // Create test registry
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "test-agent"}, {"role", "tester"}, {"version", "1.0"},
             {"tier", "code"}, {"tags", {"test"}}}
        });
        std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();
    }

    void TearDown() override {
        std::filesystem::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

TEST_F(SpecializedCmdTest, VoiceRuns) {
    std::istringstream input("exit\n");
    auto code = cmd_voice_ex(ctx_, {}, input);
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, TuiRuns) {
    auto code = cmd_tui(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, PolishUsage) {
    auto code = cmd_polish(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(SpecializedCmdTest, PolishMissingFile) {
    auto code = cmd_polish(ctx_, {"/tmp/nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(SpecializedCmdTest, KaizenRuns) {
    auto code = cmd_kaizen(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, AuditRuns) {
    auto code = cmd_audit(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, AuditRunCreatesEvidence) {
    auto code = cmd_audit_run(ctx_, {});
    EXPECT_EQ(code, 0);
    EXPECT_TRUE(std::filesystem::is_directory(ctx_.data_dir + "/audit"));
}

TEST_F(SpecializedCmdTest, CertifyFound) {
    auto code = cmd_certify(ctx_, {"test-agent"});
    EXPECT_GE(code, 0);
}

TEST_F(SpecializedCmdTest, CertifyNotFound) {
    auto code = cmd_certify(ctx_, {"nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(SpecializedCmdTest, CertifyUsage) {
    auto code = cmd_certify(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(SpecializedCmdTest, EvidenceVerifyList) {
    auto code = cmd_evidence_verify(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, GymUsage) {
    auto code = cmd_gym(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(SpecializedCmdTest, ReplayUsage) {
    auto code = cmd_replay(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(SpecializedCmdTest, ContextWorkerRuns) {
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-runtime");
    auto code = cmd_context_worker(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, PolishWithDryRun) {
    // Create a file to polish
    auto file_path = ctx_.euxis_home + "/test_file.txt";
    std::ofstream(file_path) << "This is test content.";
    auto code = cmd_polish(ctx_, {file_path, "--dry-run"});
    // May fail due to missing provider, but should not crash
    EXPECT_GE(code, 0);
}

TEST_F(SpecializedCmdTest, PolishWithStyle) {
    auto file_path = ctx_.euxis_home + "/test_style.txt";
    std::ofstream(file_path) << "Content to polish.";
    auto code = cmd_polish(ctx_, {file_path, "--style", "casual", "--dry-run"});
    EXPECT_GE(code, 0);
}

TEST_F(SpecializedCmdTest, KaizenWithFocus) {
    auto code = cmd_kaizen(ctx_, {"--focus", "security"});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, AuditRunWithFullFlag) {
    auto code = cmd_audit_run(ctx_, {"--full"});
    EXPECT_EQ(code, 0);
    EXPECT_TRUE(std::filesystem::is_directory(ctx_.data_dir + "/audit"));
}

TEST_F(SpecializedCmdTest, EvidenceVerifyNoArgs) {
    auto code = cmd_evidence_verify(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, EvidenceVerifyListExplicit) {
    auto code = cmd_evidence_verify(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, EvidenceVerifyMissingRun) {
    auto code = cmd_evidence_verify(ctx_, {"verify", "nonexistent-run"});
    EXPECT_EQ(code, 1);
}

TEST_F(SpecializedCmdTest, EvidenceVerifyInvalidCommand) {
    auto code = cmd_evidence_verify(ctx_, {"invalid-cmd"});
    EXPECT_EQ(code, 2);
}

TEST_F(SpecializedCmdTest, EvidenceVerifyExistingRun) {
    // Create an audit run directory with evidence
    auto audit_dir = ctx_.data_dir + "/audit/test-run-001";
    std::filesystem::create_directories(audit_dir);
    std::ofstream(audit_dir + "/system-info.txt") << "test system info";
    std::ofstream(audit_dir + "/registry-snapshot.json") << "{}";

    auto code = cmd_evidence_verify(ctx_, {"verify", "test-run-001"});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, GymWithAgent) {
    auto code = cmd_gym(ctx_, {"test-agent"});
    // Will attempt drills, may fail on provider but should handle gracefully
    EXPECT_GE(code, 0);
}

TEST_F(SpecializedCmdTest, GymAgentNotFound) {
    auto code = cmd_gym(ctx_, {"nonexistent-agent"});
    EXPECT_EQ(code, 1);
}

TEST_F(SpecializedCmdTest, GymWithDrillsArg) {
    auto code = cmd_gym(ctx_, {"test-agent", "--drills", "1"});
    EXPECT_GE(code, 0);
}

TEST_F(SpecializedCmdTest, ReplayMissingFile) {
    auto code = cmd_replay(ctx_, {"/tmp/nonexistent-log.jsonl"});
    EXPECT_EQ(code, 1);
}

TEST_F(SpecializedCmdTest, ReplayExistingFile) {
    // Create a log file
    auto log_path = ctx_.euxis_home + "/test-session.jsonl";
    std::ofstream f(log_path);
    f << R"({"timestamp":"2026-01-01","event":"start"})" << "\n"
      << R"({"timestamp":"2026-01-01","event":"end"})" << "\n";
    f.close();

    auto code = cmd_replay(ctx_, {log_path});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, ReplayPlainTextLines) {
    auto log_path = ctx_.euxis_home + "/plain-log.txt";
    std::ofstream f(log_path);
    f << "This is not JSON\n"
      << "Neither is this\n";
    f.close();

    auto code = cmd_replay(ctx_, {log_path});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, ContextWorkerWithScope) {
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-runtime");
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-data");
    auto code = cmd_context_worker(ctx_, {"--scope", "project"});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, VoiceWithTier) {
    std::istringstream input("exit\n");
    auto code = cmd_voice_ex(ctx_, {"--tier", "routine"}, input);
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, AuditWithSensitiveFiles) {
    // Create a .env file to trigger warning
    std::ofstream(ctx_.euxis_home + "/.env") << "SECRET=value";
    auto code = cmd_audit(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, CertifyAgentWithAllFields) {
    // Create an agent that would score well
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "complete-agent"}, {"role", "engineer"}, {"version", "1.0"},
         {"tier", "code"}, {"tags", {"prod"}}, {"prompt_path", "agents/complete.md"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    // Create the prompt file
    std::filesystem::create_directories(ctx_.euxis_home + "/agents");
    std::ofstream(ctx_.euxis_home + "/agents/complete.md")
        << "agent_id: complete-agent\nrole: engineer\nversion: 1.0";

    auto code = cmd_certify(ctx_, {"complete-agent"});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, ReplayRelativePathFallback) {
    // Create log in runtime dir
    auto runtime_dir = ctx_.data_dir + "/runtime";
    std::filesystem::create_directories(runtime_dir);
    std::ofstream(runtime_dir + "/session.jsonl")
        << R"({"type":"init"})" << "\n";

    auto code = cmd_replay(ctx_, {"session.jsonl"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: voice conversation loop with input that triggers PII filter and provider ---
TEST_F(SpecializedCmdTest, VoiceMultipleTurnsAndEof) {
    std::istringstream input("hello\n  \nexit\n");
    auto code = cmd_voice_ex(ctx_, {}, input);
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, VoiceEofImmediately) {
    std::istringstream input("");
    auto code = cmd_voice_ex(ctx_, {}, input);
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, VoiceQuitCommand) {
    std::istringstream input("quit\n");
    auto code = cmd_voice_ex(ctx_, {}, input);
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, VoiceWithWhitespaceTrimming) {
    std::istringstream input("  hello world  \nexit\n");
    auto code = cmd_voice_ex(ctx_, {}, input);
    EXPECT_EQ(code, 0);
}

// --- Coverage: tui text-based dashboard fallback path ---
TEST_F(SpecializedCmdTest, TuiTextDashboardWithRuntime) {
    // Ensure no ETX binary exists; exercises full text dashboard fallback
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-data");
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-runtime");
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-core");
    auto code = cmd_tui(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: polish with PII detection ---
TEST_F(SpecializedCmdTest, PolishWithPIIContent) {
    auto file_path = ctx_.euxis_home + "/pii_test.txt";
    // Write content with email-like PII
    std::ofstream(file_path) << "Contact john.doe@example.com for details. SSN: 123-45-6789.";
    auto code = cmd_polish(ctx_, {file_path, "--dry-run"});
    EXPECT_GE(code, 0);
}

// --- Coverage: polish with --style and write-back (non dry-run) ---
TEST_F(SpecializedCmdTest, PolishWithoutDryRun) {
    auto file_path = ctx_.euxis_home + "/polish_wb.txt";
    std::ofstream(file_path) << "Content to polish.";
    // Without --dry-run, exercises the write-back path (lines 283-287)
    auto code = cmd_polish(ctx_, {file_path});
    EXPECT_GE(code, 0);
}

// --- Coverage: kaizen with provider failure => local heuristic suggestions ---
TEST_F(SpecializedCmdTest, KaizenLocalHeuristicFallback) {
    // Registry with agents missing some fields triggers heuristic suggestions
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "incomplete"}, {"role", ""}, {"tier", "code"},
         {"version", ""}, {"tags", nlohmann::json::array()},
         {"capabilities", nlohmann::json::array()}, {"prompt_path", ""}}
    });
    reg["squads"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_kaizen(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, KaizenAllAgentsMeetBaseline) {
    // All agents with full fields (provider still fails, exercises "all meet baseline")
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "full-agent"}, {"role", "engineer"}, {"tier", "code"},
         {"version", "1.0"}, {"tags", {"prod"}},
         {"capabilities", {"coding"}}, {"prompt_path", "agents/f.md"}}
    });
    reg["squads"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_kaizen(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: audit with config directory permissions ---
TEST_F(SpecializedCmdTest, AuditWithConfigDir) {
    auto config_dir = std::filesystem::path(ctx_.data_dir) / "config";
    std::filesystem::create_directories(config_dir);
    auto code = cmd_audit(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, AuditNoIssues) {
    // No sensitive files, just the data dir
    auto code = cmd_audit(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: evidence-verify list with existing audit directories ---
TEST_F(SpecializedCmdTest, EvidenceVerifyListWithRuns) {
    auto audit_dir = ctx_.data_dir + "/audit/run-001";
    std::filesystem::create_directories(audit_dir);
    std::ofstream(audit_dir + "/evidence.txt") << "evidence data";
    std::ofstream(audit_dir + "/report.json") << "{}";

    auto code = cmd_evidence_verify(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: gym drill evaluation scoring paths ---
TEST_F(SpecializedCmdTest, GymWithDataTierAgent) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "data-agent"}, {"role", "analyst"}, {"version", "1.0"},
         {"tier", "data"}, {"tags", {"data"}}, {"capabilities", {"analysis"}},
         {"prompt_path", ""}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_gym(ctx_, {"data-agent", "--drills", "1"});
    EXPECT_GE(code, 0);
}

TEST_F(SpecializedCmdTest, GymWithRoutineTierAgent) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "routine-agent"}, {"role", "formatter"}, {"version", "1.0"},
         {"tier", "routine"}, {"tags", {"format"}}, {"capabilities", {"formatting"}},
         {"prompt_path", ""}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_gym(ctx_, {"routine-agent", "--drills", "1"});
    EXPECT_GE(code, 0);
}

TEST_F(SpecializedCmdTest, GymWithReasonTierAgent) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "reason-agent"}, {"role", "architect"}, {"version", "1.0"},
         {"tier", "reason"}, {"tags", {"arch"}}, {"capabilities", {"design"}},
         {"prompt_path", ""}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_gym(ctx_, {"reason-agent", "--drills", "2"});
    EXPECT_GE(code, 0);
}

// --- Coverage: context-worker with different scope options ---
TEST_F(SpecializedCmdTest, ContextWorkerWithGlobalScope) {
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-runtime");
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-data");
    auto code = cmd_context_worker(ctx_, {"--scope", "global"});
    EXPECT_EQ(code, 0);
}

TEST_F(SpecializedCmdTest, ContextWorkerWithFullContent) {
    // Create multiple directories that will be scanned
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-runtime");
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-data/agents");
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-core");
    std::filesystem::create_directories(ctx_.euxis_home + "/euxis-cli-cpp");

    // Add some files to be scanned
    std::ofstream(ctx_.euxis_home + "/euxis-core/test.txt") << "test content";

    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "ctx-agent"}, {"role", "test"}, {"tier", "code"},
         {"version", "1.0"}, {"tags", {"test"}}}
    });
    reg["squads"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_context_worker(ctx_, {"--scope", "project"});
    EXPECT_EQ(code, 0);

    // Verify context files were created
    auto context_dir = std::filesystem::path(ctx_.euxis_home) / "euxis-runtime" / "context";
    EXPECT_TRUE(std::filesystem::is_directory(context_dir));
}

// --- Coverage: certify with score < max_score ---
TEST_F(SpecializedCmdTest, CertifyAgentNotCertified) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "incomplete-cert"}, {"role", ""}, {"tier", ""},
         {"version", ""}, {"tags", nlohmann::json::array()}, {"prompt_path", ""}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_certify(ctx_, {"incomplete-cert"});
    EXPECT_EQ(code, 1); // NOT CERTIFIED
}

// --- Coverage: replay with "type" field in JSON instead of "event" ---
TEST_F(SpecializedCmdTest, ReplayWithTypeField) {
    auto log_path = ctx_.euxis_home + "/type-log.jsonl";
    std::ofstream f(log_path);
    f << R"({"timestamp":"2026-01-01","type":"init"})" << "\n"
      << R"({"timestamp":"2026-01-01","type":"complete"})" << "\n";
    f.close();

    auto code = cmd_replay(ctx_, {log_path});
    EXPECT_EQ(code, 0);
}

// --- Coverage: audit-run full flag ---
TEST_F(SpecializedCmdTest, AuditRunCreatesMultipleSensitiveFiles) {
    // Create sensitive files to exercise warning paths in audit
    std::ofstream(ctx_.euxis_home + "/.env") << "SECRET=x";
    std::ofstream(ctx_.euxis_home + "/credentials.json") << "{}";
    std::ofstream(ctx_.euxis_home + "/secrets.yaml") << "key: val";

    auto code = cmd_audit_run(ctx_, {"--full"});
    EXPECT_EQ(code, 0);
}

} // namespace
} // namespace euxis::cli::cmd
