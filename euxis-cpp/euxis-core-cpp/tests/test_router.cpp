#include <gtest/gtest.h>

#include "euxis/core/router.hpp"

namespace euxis::core {
namespace {

TEST(FinOpsRouterTest, LowComplexityUsesOllama) {
    FinOpsRouter r;
    EXPECT_EQ(r.select_provider("low"), "ollama");
}

TEST(FinOpsRouterTest, SpeedPrioritySelectsFastest) {
    FinOpsRouter r;
    auto provider = r.select_provider("high", "speed");
    EXPECT_EQ(provider, "groq");
}

TEST(FinOpsRouterTest, CostPrioritySelectsCheapest) {
    FinOpsRouter r;
    auto provider = r.select_provider("high", "cost");
    EXPECT_EQ(provider, "ollama");
}

TEST(FinOpsRouterTest, TrackUsageUpdatesSpend) {
    FinOpsRouter r;
    r.track_usage("anthropic", 1000);
    EXPECT_GT(r.current_spend(), 0.0);
}

TEST(FinOpsRouterTest, BalancedSelection) {
    FinOpsRouter r;
    auto provider = r.select_provider("medium", "balanced");
    EXPECT_FALSE(provider.empty());
}

} // namespace
} // namespace euxis::core
