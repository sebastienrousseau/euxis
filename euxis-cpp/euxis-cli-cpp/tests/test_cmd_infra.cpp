#include <gtest/gtest.h>
#include "euxis/cli/cmd/infra.hpp"

#include <filesystem>

namespace euxis::cli::cmd {
namespace {

class InfraCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_infra_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/euxis-data";
        std::filesystem::create_directories(ctx_.data_dir + "/config");
    }

    void TearDown() override {
        std::filesystem::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

TEST_F(InfraCmdTest, BusStatus) {
    auto code = cmd_bus(ctx_, {"status"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, BusUsage) {
    auto code = cmd_bus(ctx_, {});
    EXPECT_EQ(code, 0); // status is default
}

TEST_F(InfraCmdTest, DaemonStatus) {
    auto code = cmd_daemon(ctx_, {"status"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, DaemonUsage) {
    auto code = cmd_daemon(ctx_, {});
    EXPECT_EQ(code, 0); // status is default
}

TEST_F(InfraCmdTest, DeployUsage) {
    auto code = cmd_deploy(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(InfraCmdTest, DeployMissingFile) {
    auto code = cmd_deploy(ctx_, {"/tmp/nonexistent.json"});
    EXPECT_EQ(code, 1);
}

TEST_F(InfraCmdTest, OptimizeRuns) {
    auto code = cmd_optimize(ctx_, {});
    EXPECT_EQ(code, 0);
}

} // namespace
} // namespace euxis::cli::cmd
