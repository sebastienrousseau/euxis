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

TEST(FinOpsRouterTest, ProviderIndexLookup) {
    FinOpsRouter r;
    // track_usage uses the name_to_provider_ index internally
    r.track_usage("groq", 1000);
    EXPECT_GT(r.current_spend(), 0.0);
    
    double spend_before = r.current_spend();
    r.track_usage("unknown", 1000);
    EXPECT_DOUBLE_EQ(r.current_spend(), spend_before);
}

TEST(FinOpsRouterTest, TrackUsageUnknownProvider) {
    FinOpsRouter r;
    double before = r.current_spend();
    r.track_usage("nonexistent-provider", 1000);
    // Unknown provider should not change spend
    EXPECT_DOUBLE_EQ(r.current_spend(), before);
}

TEST(FinOpsRouterTest, TrackUsageMultipleProviders) {
    FinOpsRouter r;
    r.track_usage("anthropic", 1000);
    double after_first = r.current_spend();
    r.track_usage("openai", 1000);
    EXPECT_GT(r.current_spend(), after_first);
}

TEST(FinOpsRouterTest, TrackUsageOllama) {
    FinOpsRouter r;
    r.track_usage("ollama", 5000);
    // Ollama is free (cost_per_1k_tokens = 0)
    EXPECT_DOUBLE_EQ(r.current_spend(), 0.0);
}

TEST(FinOpsRouterTest, SessionUsageTracking) {
    FinOpsRouter r;
    r.track_session_usage("sess-1", "agent-1", "anthropic-model", 100, 200);
    EXPECT_GT(r.session_cost("sess-1"), 0.0);
}

TEST(FinOpsRouterTest, SessionUsageWithUnknownModel) {
    FinOpsRouter r;
    r.track_session_usage("sess-2", "agent-1", "unknown-model", 100, 200);
    // Should use default estimate
    EXPECT_GT(r.session_cost("sess-2"), 0.0);
}

TEST(FinOpsRouterTest, SessionCostUnknownSession) {
    FinOpsRouter r;
    EXPECT_DOUBLE_EQ(r.session_cost("nonexistent"), 0.0);
}

TEST(FinOpsRouterTest, CheckBudgetUnderLimit) {
    FinOpsRouter r;
    r.track_session_usage("sess-1", "agent-1", "ollama-model", 100, 100);
    EXPECT_TRUE(r.check_budget("sess-1", 100.0));
}

TEST(FinOpsRouterTest, CheckBudgetOverLimit) {
    FinOpsRouter r;
    r.track_session_usage("sess-1", "agent-1", "anthropic-model", 1000000, 1000000);
    EXPECT_FALSE(r.check_budget("sess-1", 0.001));
}

TEST(FinOpsRouterTest, CustomBudgetLimit) {
    FinOpsRouter r(0.01);
    r.track_usage("anthropic", 1000);
    EXPECT_GT(r.current_spend(), 0.0);
}

TEST(FinOpsRouterTest, HighComplexityDefaultSelection) {
    FinOpsRouter r;
    auto provider = r.select_provider("high");
    EXPECT_FALSE(provider.empty());
    // With default priority, should use weighted scoring
}

TEST(FinOpsRouterTest, MultipleSessionUsageAccumulates) {
    FinOpsRouter r;
    r.track_session_usage("sess-1", "a1", "anthropic-x", 100, 100);
    r.track_session_usage("sess-1", "a2", "anthropic-x", 200, 200);
    double cost = r.session_cost("sess-1");
    // Two entries, cost should be positive
    EXPECT_GT(cost, 0.0);
}

TEST(FinOpsRouterTest, SessionEviction) {
    FinOpsRouter r;
    // Create 100 sessions
    for (int i = 0; i < 100; ++i) {
        r.track_session_usage("s-" + std::to_string(i), "a", "ollama", 1, 1);
    }
    EXPECT_GT(r.session_cost("s-0"), 0.0);
    
    // Add 101st session, should evict s-0
    r.track_session_usage("s-100", "a", "ollama", 1, 1);
    EXPECT_DOUBLE_EQ(r.session_cost("s-0"), 0.0);
    EXPECT_GT(r.session_cost("s-100"), 0.0);
}

TEST(FinOpsRouterTest, SessionLRURefresh) {
    FinOpsRouter r;
    // Fill up
    for (int i = 0; i < 100; ++i) r.track_session_usage("s-" + std::to_string(i), "a", "ollama", 1, 1);
    
    // Use s-0 again, it should move to the back of the eviction queue
    r.track_session_usage("s-0", "a", "ollama", 1, 1);
    
    // Add new session, should NOT evict s-0 now, it should evict s-1
    r.track_session_usage("s-new", "a", "ollama", 1, 1);
    EXPECT_GT(r.session_cost("s-0"), 0.0);
    EXPECT_DOUBLE_EQ(r.session_cost("s-1"), 0.0);
}

} // namespace
} // namespace euxis::core
