#include <gtest/gtest.h>
#include "euxis/cli/provider_router.hpp"

#include <cstdlib>

namespace euxis::cli {
namespace {

TEST(ProviderRouterTest, TierLabel) {
    EXPECT_EQ(tier_label(Tier::Routine), "routine");
    EXPECT_EQ(tier_label(Tier::Data), "data");
    EXPECT_EQ(tier_label(Tier::Code), "code");
    EXPECT_EQ(tier_label(Tier::Reason), "reason");
}

TEST(ProviderRouterTest, ParseTier) {
    EXPECT_EQ(parse_tier("routine"), Tier::Routine);
    EXPECT_EQ(parse_tier("data"), Tier::Data);
    EXPECT_EQ(parse_tier("code"), Tier::Code);
    EXPECT_EQ(parse_tier("reason"), Tier::Reason);
    EXPECT_EQ(parse_tier("unknown"), Tier::Code); // default
}

TEST(ProviderRouterTest, SelectModel) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Code);
    EXPECT_FALSE(sel.model.empty());
    EXPECT_EQ(sel.tier, Tier::Code);
}

TEST(ProviderRouterTest, SelectModelCostIncreases) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto routine = router.select_model(Tier::Routine);
    auto data = router.select_model(Tier::Data);
    auto code = router.select_model(Tier::Code);
    auto reason = router.select_model(Tier::Reason);

    EXPECT_LT(routine.estimated_cost_per_1m, data.estimated_cost_per_1m);
    EXPECT_LT(data.estimated_cost_per_1m, code.estimated_cost_per_1m);
    EXPECT_LT(code.estimated_cost_per_1m, reason.estimated_cost_per_1m);
}

TEST(ProviderRouterTest, AnalyzeTaskTier) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    EXPECT_EQ(router.analyze_task_tier("architect a new system"), Tier::Reason);
    EXPECT_EQ(router.analyze_task_tier("implement the feature"), Tier::Code);
    EXPECT_EQ(router.analyze_task_tier("analyze the data"), Tier::Data);
    EXPECT_EQ(router.analyze_task_tier("hello world"), Tier::Routine);
}

TEST(ProviderRouterTest, RouteUsesMaxTier) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto result = router.route("routine", "design a complex architecture");
    EXPECT_EQ(result.tier, Tier::Reason);
}

TEST(ProviderRouterTest, DetectProvider) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto provider = router.detect_provider();
    EXPECT_FALSE(provider.empty());
}

TEST(ProviderRouterTest, EnvOverride) {
    setenv("EUXIS_MODEL_OVERRIDE", "test-model-123", 1);
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Code);
    EXPECT_EQ(sel.model, "test-model-123");
    unsetenv("EUXIS_MODEL_OVERRIDE");
}

} // namespace
} // namespace euxis::cli
