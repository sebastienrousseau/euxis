#include <gtest/gtest.h>
#include "euxis/core/router.hpp"

namespace euxis::core {
namespace {

TEST(FinOpsRouterTest, SelectionLogic) {
    FinOpsRouter r;
    // Low complexity path
    EXPECT_EQ(r.select_provider("low"), "ollama");
    
    // Speed priority - ollama (150ms) is the current fastest in the list
    EXPECT_EQ(r.select_provider("high", "speed"), "ollama");
    
    // Cost priority - ollama (0.0) should win
    EXPECT_EQ(r.select_provider("high", "cost"), "ollama");
    
    // Balanced weighted scoring - verify it returns a valid hardcoded provider
    auto best = r.select_provider("high", "balanced");
    EXPECT_TRUE(best == "ollama" || best == "claude" || best == "gemini");
}

TEST(FinOpsRouterTest, UsageTracking) {
    FinOpsRouter r;
    // Use a known provider from the current implementation
    r.track_usage("claude", 1000);
    EXPECT_GT(r.current_spend(), 0.0);
    
    // Unknown provider warning path
    r.track_usage("nonexistent", 1000);
}

TEST(FinOpsRouterTest, SessionManagement) {
    FinOpsRouter r;
    // Use a known provider model name pattern
    r.track_session_usage("s1", "a1", "claude-model", 100, 100);
    EXPECT_GT(r.session_cost("s1"), 0.0);
    EXPECT_TRUE(r.check_budget("s1", 1.0));
    EXPECT_FALSE(r.check_budget("s1", 0.000001));
    
    // Unknown session cost
    EXPECT_DOUBLE_EQ(r.session_cost("unknown"), 0.0);
}

TEST(FinOpsRouterTest, EvictionAndLRU) {
    FinOpsRouter r;
    // Fill up to 100 (session_limit_ defaults to 100)
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
    FinOpsRouter r;
    // Balanced selection should return something default (ollama) even if weird input
    EXPECT_FALSE(r.select_provider("medium", "unknown_priority").empty());
}

// --- F2: swarm priority exercises the round-robin atomic counter ---
TEST(FinOpsRouterTest, SwarmPriorityRoundRobinsAcrossProviders) {
    FinOpsRouter r;
    std::set<std::string> seen;
    // 3 providers; iterate N>3 times to guarantee every slot is visited.
    for (int i = 0; i < 9; ++i) {
        seen.insert(r.select_provider("high", "swarm"));
    }
    EXPECT_EQ(seen.size(), 3u) << "swarm priority must cycle through all providers";
    EXPECT_TRUE(seen.count("ollama"));
    EXPECT_TRUE(seen.count("claude"));
    EXPECT_TRUE(seen.count("gemini"));
}

// --- F3: track_session_usage with a model name that matches no known
//         provider exercises the cost-fallback branch (router.cpp:107-109). ---
TEST(FinOpsRouterTest, UnknownModelStillProducesNonZeroCost) {
    FinOpsRouter r;
    r.track_session_usage("s-unknown", "agent-x", "totally-fictional-model",
                          250, 250);
    EXPECT_GT(r.session_cost("s-unknown"), 0.0);
}

// --- F3 cont'd: zero-token call still costs the floor (max(1, total)) ---
TEST(FinOpsRouterTest, ZeroTokensFallbackHitsFloor) {
    FinOpsRouter r;
    r.track_session_usage("s-zero", "agent-x", "no-match-model", 0, 0);
    EXPECT_GT(r.session_cost("s-zero"), 0.0);
}

} // namespace
} // namespace euxis::core
