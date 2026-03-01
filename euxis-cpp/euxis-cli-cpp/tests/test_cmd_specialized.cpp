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
    auto code = cmd_voice(ctx_, {});
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

} // namespace
} // namespace euxis::cli::cmd
