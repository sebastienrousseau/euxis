#include <gtest/gtest.h>
#include "euxis/cli/cli_provider.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace euxis::cli {
namespace {

namespace fs = std::filesystem;

class CliProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/euxis_test_cli_provider_" + std::to_string(getpid());
        fs::create_directories(test_dir_ + "/config");
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(CliProviderTest, ConstructorCreatesInstance) {
    CliProvider provider(test_dir_);
    SUCCEED();
}

TEST_F(CliProviderTest, RouteReturnsModelSpec) {
    CliProvider provider(test_dir_);
    auto spec = provider.route("code", "test task");

    EXPECT_FALSE(spec.provider.empty());
    EXPECT_FALSE(spec.model.empty());
    EXPECT_FALSE(spec.tier.empty());
}

TEST_F(CliProviderTest, RouteRoutineTier) {
    CliProvider provider(test_dir_);
    auto spec = provider.route("routine", "simple task");

    EXPECT_FALSE(spec.provider.empty());
    EXPECT_FALSE(spec.model.empty());
}

TEST_F(CliProviderTest, RouteReasonTier) {
    CliProvider provider(test_dir_);
    auto spec = provider.route("reason", "complex analysis");

    EXPECT_FALSE(spec.provider.empty());
    EXPECT_FALSE(spec.model.empty());
}

TEST_F(CliProviderTest, ExecuteWithUnknownProvider) {
    CliProvider provider(test_dir_);

    runtime::ModelSpec spec;
    spec.provider = "nonexistent_provider_xyz";
    spec.model = "test-model";
    spec.tier = "routine";

    auto result = provider.execute(spec, "hello", 5);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST_F(CliProviderTest, ExecuteReturnsDuration) {
    CliProvider provider(test_dir_);

    runtime::ModelSpec spec;
    spec.provider = "nonexistent_provider_xyz";
    spec.model = "test-model";
    spec.tier = "code";

    auto result = provider.execute(spec, "hello", 5);
    EXPECT_GE(result.duration_ms, 0.0);
}

TEST_F(CliProviderTest, SwitchModelAlwaysSucceeds) {
    CliProvider provider(test_dir_);

    runtime::ModelSpec spec;
    spec.provider = "claude";
    spec.model = "claude-sonnet-4-6";
    spec.tier = "code";

    EXPECT_TRUE(provider.switch_model(spec));
}

TEST_F(CliProviderTest, SwitchModelEmptySpec) {
    CliProvider provider(test_dir_);

    runtime::ModelSpec spec;
    EXPECT_TRUE(provider.switch_model(spec));
}

} // namespace
} // namespace euxis::cli
