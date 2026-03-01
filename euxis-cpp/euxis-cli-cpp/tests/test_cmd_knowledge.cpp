#include <gtest/gtest.h>
#include "euxis/cli/cmd/knowledge.hpp"

#include <filesystem>
#include <fstream>

namespace euxis::cli::cmd {
namespace {

class KnowledgeCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_know_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/euxis-data";
        std::filesystem::create_directories(ctx_.data_dir + "/config");
    }

    void TearDown() override {
        std::filesystem::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

TEST_F(KnowledgeCmdTest, CortexUsage) {
    auto code = cmd_cortex(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CortexStatsNoDb) {
    // stats now always succeeds (reports 0 entries/files when empty)
    auto code = cmd_cortex(ctx_, {"stats"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, GraphShow) {
    auto code = cmd_graph(ctx_, {"show"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, GraphUsage) {
    auto code = cmd_graph(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CodexListNoFile) {
    auto code = cmd_codex(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CodexValidateNoFile) {
    auto code = cmd_codex(ctx_, {"validate"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, OmnigraphRuns) {
    auto code = cmd_omnigraph(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, SlashList) {
    auto code = cmd_slash(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, SlashUsage) {
    auto code = cmd_slash(ctx_, {});
    EXPECT_EQ(code, 0); // list is default
}

} // namespace
} // namespace euxis::cli::cmd
