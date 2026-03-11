#include <gtest/gtest.h>
#include "euxis/core/router.hpp"

namespace euxis::core {
namespace {

TEST(FinOpsRouterTest, SelectionLogic) {
    FinOpsRouter r;
    // Low complexity path
    EXPECT_EQ(r.select_provider("low"), "ollama");
    
    // Speed priority
    EXPECT_EQ(r.select_provider("high", "speed"), "groq");
    
    // Cost priority
    EXPECT_EQ(r.select_provider("high", "cost"), "ollama");
    
    // Balanced weighted scoring
    EXPECT_FALSE(r.select_provider("high", "balanced").empty());
}

TEST(FinOpsRouterTest, UsageTracking) {
    FinOpsRouter r;
    r.track_usage("groq", 1000);
    EXPECT_GT(r.current_spend(), 0.0);
    
    // Unknown provider warning path
    r.track_usage("nonexistent", 1000);
}

TEST(FinOpsRouterTest, SessionManagement) {
    FinOpsRouter r;
    r.track_session_usage("s1", "a1", "groq-model", 100, 100);
    EXPECT_GT(r.session_cost("s1"), 0.0);
    EXPECT_TRUE(r.check_budget("s1", 1.0));
    EXPECT_FALSE(r.check_budget("s1", 0.000001));
    
    // Unknown session cost
    EXPECT_DOUBLE_EQ(r.session_cost("unknown"), 0.0);
}

TEST(FinOpsRouterTest, EvictionAndLRU) {
    FinOpsRouter r;
    // Fill up to 100
    for (int i = 0; i < 100; ++i) r.track_session_usage("s" + std::to_string(i), "a", "ollama", 100, 100);
    EXPECT_GT(r.session_cost("s0"), 0.0);
    
    // Refresh s0
    r.track_session_usage("s0", "a", "ollama", 100, 100);
    
    // Add 101st session - should evict s1 (oldest after s0 refresh)
    r.track_session_usage("s101", "a", "ollama", 100, 100);
    EXPECT_GT(r.session_cost("s0"), 0.0);
    EXPECT_DOUBLE_EQ(r.session_cost("s1"), 0.0);
}

TEST(FinOpsRouterTest, EmptyProviderScoring) {
    // We can't easily empty the providers since they are set in constructor,
    // but we can test the default fallback if no match is found.
    FinOpsRouter r;
    // Balanced selection should return something default (ollama) even if weird input
    EXPECT_FALSE(r.select_provider("medium", "unknown_priority").empty());
}

} // namespace
} // namespace euxis::core
