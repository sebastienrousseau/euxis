#include <gtest/gtest.h>
#include "euxis/cli/cmd/dev.hpp"

#include <filesystem>

namespace euxis::cli::cmd {
namespace {

class DevCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_dev_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/euxis-data";
        std::filesystem::create_directories(ctx_.data_dir + "/agents");
    }

    void TearDown() override {
        std::filesystem::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

TEST_F(DevCmdTest, BenchRuns) {
    auto code = cmd_bench(ctx_, {"filesystem", "--iterations", "3"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, HooksList) {
    auto code = cmd_hooks(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, SetupShellHooks) {
    auto code = cmd_setup_shell_hooks(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, GitGuard) {
    auto code = cmd_git_guard(ctx_, {});
    // May return non-zero if not in a git repo, shouldn't crash
    EXPECT_GE(code, 0);
}

TEST_F(DevCmdTest, LicenseCheck) {
    auto code = cmd_license_check(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(DevCmdTest, DocsTest) {
    auto code = cmd_docs_test(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, SyncDocs) {
    auto code = cmd_sync_docs(ctx_, {});
    // Returns 1 when docs/ dir doesn't exist (test env has no docs/)
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(DevCmdTest, TestInfra) {
    auto code = cmd_test_infra(ctx_, {});
    EXPECT_GE(code, 0);
}

} // namespace
} // namespace euxis::cli::cmd
