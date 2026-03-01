#include <gtest/gtest.h>
#include "euxis/cli/cmd/system.hpp"

#include <filesystem>

namespace euxis::cli::cmd {
namespace {

class SystemCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_sys_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/euxis-data";
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

} // namespace
} // namespace euxis::cli::cmd
