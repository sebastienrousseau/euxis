#include <gtest/gtest.h>
#include <filesystem>

#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/provider_executor.hpp"

namespace euxis::integration {
namespace {

class CliDispatchTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_integ_cli";
        std::filesystem::remove_all(tmp_);
        std::filesystem::create_directories(tmp_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_);
    }
};

TEST_F(CliDispatchTest, RouterDetectsProvider) {
    cli::ProviderRouter router(tmp_.string());
    auto provider = router.detect_provider();
    // Should return a non-empty string (auto-detected or "local")
    EXPECT_FALSE(provider.empty());
}

TEST_F(CliDispatchTest, TierAnalysisClassifiesTask) {
    cli::ProviderRouter router(tmp_.string());

    auto simple_tier = router.analyze_task_tier("list files");
    EXPECT_EQ(simple_tier, cli::Tier::Routine);

    auto complex_tier = router.analyze_task_tier("refactor the authentication module with OAuth2");
    EXPECT_EQ(complex_tier, cli::Tier::Code);
}

TEST_F(CliDispatchTest, ModelSelectionReturnsTierMatch) {
    cli::ProviderRouter router(tmp_.string());

    auto routine = router.select_model(cli::Tier::Routine);
    EXPECT_FALSE(routine.model.empty());
    EXPECT_EQ(routine.tier, cli::Tier::Routine);

    auto reason = router.select_model(cli::Tier::Reason);
    EXPECT_FALSE(reason.model.empty());
    EXPECT_EQ(reason.tier, cli::Tier::Reason);
    EXPECT_GE(reason.estimated_cost_per_1m, routine.estimated_cost_per_1m);
}

TEST_F(CliDispatchTest, PromptBuilding) {
    auto prompt = cli::ProviderExecutor::build_prompt(
        "You are a helpful assistant.",
        "Write a hello world program",
        "Language: C++"
    );
    EXPECT_NE(prompt.find("helpful assistant"), std::string::npos);
    EXPECT_NE(prompt.find("hello world"), std::string::npos);
    EXPECT_NE(prompt.find("C++"), std::string::npos);
}

TEST_F(CliDispatchTest, ErrorClassification) {
    // Rate limit
    auto rate = cli::ProviderExecutor::classify_error(429, "rate_limit_exceeded");
    ASSERT_TRUE(rate.has_value());

    // Auth failure
    auto auth = cli::ProviderExecutor::classify_error(401, "invalid_api_key");
    ASSERT_TRUE(auth.has_value());

    // Success is not an error
    auto ok = cli::ProviderExecutor::classify_error(200, "ok");
    EXPECT_FALSE(ok.has_value());
}

TEST_F(CliDispatchTest, FallbackChainNonEmpty) {
    cli::ProviderRouter router(tmp_.string());
    auto selection = router.select_model(cli::Tier::Code);
    auto chain = router.model_fallback_chain(selection.model);
    // Fallback chain should include at least the original model
    EXPECT_GE(chain.size(), 1u);
}

TEST_F(CliDispatchTest, RoutingByAgentTier) {
    cli::ProviderRouter router(tmp_.string());
    auto selection = router.route("code", "implement a feature");
    EXPECT_FALSE(selection.model.empty());
    EXPECT_FALSE(selection.provider.empty());
}

} // namespace
} // namespace euxis::integration
