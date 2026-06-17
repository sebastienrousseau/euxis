/// @file
/// @brief Tests for the pluggable context-compaction strategy.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "euxis/runtime/context_engine.hpp"
#include "euxis/runtime/agent_session.hpp"
#include "euxis/runtime/session_store.hpp"

namespace euxis::runtime {
namespace {

// Build a ConversationMessage with content of the requested length so tests
// can drive estimate_tokens() deterministically.
auto msg(Role role, std::size_t content_bytes) -> ConversationMessage {
    SessionMessage sm{};
    sm.role = role;
    sm.content = std::string(content_bytes, 'x');
    sm.agent_id = "test";
    sm.model = "test-model";
    sm.timestamp = "2026-01-01T00:00:00Z";
    sm.duration_ms = 0.0;
    return ConversationMessage{sm};
}

auto messages(std::size_t count, std::size_t bytes_each)
    -> std::vector<ConversationMessage> {
    std::vector<ConversationMessage> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto role = (i % 2 == 0) ? Role::User : Role::Assistant;
        out.push_back(msg(role, bytes_each));
    }
    return out;
}

// ---------------------------------------------------------------------------
// CompactionPlan helpers
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, EmptyPlanReportsAsEmpty) {
    CompactionPlan p;
    EXPECT_TRUE(p.is_empty());
    EXPECT_EQ(p.compact_count(), 0u);
}

TEST(ContextEngineTest, NonEmptyPlanReportsRange) {
    CompactionPlan p;
    p.compact_start = 4;
    p.compact_end   = 11;
    EXPECT_FALSE(p.is_empty());
    EXPECT_EQ(p.compact_count(), 7u);
}

// ---------------------------------------------------------------------------
// estimate_tokens
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, EstimateTokensEmptyVectorIsZero) {
    EXPECT_EQ(estimate_tokens({}), 0u);
}

TEST(ContextEngineTest, EstimateTokensFourBytesPerToken) {
    // 100 messages × 400 bytes each = 40,000 bytes → 10,000 tokens
    auto m = messages(100, 400);
    EXPECT_EQ(estimate_tokens(m), 10000u);
}

TEST(ContextEngineTest, EstimateTokensSubByteFractionsTruncate) {
    // 1 message × 3 bytes → 0 tokens (3 / 4 = 0)
    auto m = messages(1, 3);
    EXPECT_EQ(estimate_tokens(m), 0u);
}

// ---------------------------------------------------------------------------
// Default config matches Hermes
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, DefaultConfigMatchesHermes) {
    WindowedContextEngine eng;
    EXPECT_EQ(eng.config().keep_head_messages,  4u);
    EXPECT_EQ(eng.config().keep_tail_messages,  6u);
    EXPECT_DOUBLE_EQ(eng.config().trigger_ratio, 0.75);
}

// ---------------------------------------------------------------------------
// Below trigger threshold → empty plan
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, BelowThresholdReturnsEmptyPlan) {
    WindowedContextEngine eng;
    auto m = messages(20, 100);
    // 1000 tokens estimated, window 8192, threshold 6144 → below.
    auto plan = eng.plan(m, 1000, 8192);
    EXPECT_TRUE(plan.is_empty());
}

// ---------------------------------------------------------------------------
// Above trigger threshold and enough messages → non-empty plan covering
// the middle range.
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, AboveThresholdComputesMiddleRange) {
    WindowedContextEngine eng;
    auto m = messages(20, 100);
    // 7000 / 8192 > 0.75 → fires. 20 - 4 - 6 = 10 messages compacted.
    auto plan = eng.plan(m, 7000, 8192);
    EXPECT_FALSE(plan.is_empty());
    EXPECT_EQ(plan.keep_head,     4u);
    EXPECT_EQ(plan.keep_tail,     6u);
    EXPECT_EQ(plan.compact_start, 4u);
    EXPECT_EQ(plan.compact_end,   14u);
    EXPECT_EQ(plan.compact_count(), 10u);
    EXPECT_EQ(plan.summary_role,  "system");
}

// ---------------------------------------------------------------------------
// Not enough messages to compact → empty plan even when over threshold
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, NotEnoughMessagesReturnsEmptyEvenAboveThreshold) {
    WindowedContextEngine eng;
    // head + tail = 10; only 9 messages.
    auto m = messages(9, 1000);
    auto plan = eng.plan(m, 999999, 8192);
    EXPECT_TRUE(plan.is_empty());
}

TEST(ContextEngineTest, ExactlyHeadPlusTailReturnsEmpty) {
    WindowedContextEngine eng;
    auto m = messages(10, 1000); // head + tail = 10; nothing in the middle.
    auto plan = eng.plan(m, 999999, 8192);
    EXPECT_TRUE(plan.is_empty());
}

// ---------------------------------------------------------------------------
// Custom config is honoured
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, CustomConfigChangesBoundaries) {
    WindowedContextEngine::Config cfg{};
    cfg.keep_head_messages = 2;
    cfg.keep_tail_messages = 3;
    cfg.trigger_ratio      = 0.5;
    WindowedContextEngine eng{cfg};

    auto m = messages(10, 100);
    // 4500 / 8192 > 0.5. 10 - 2 - 3 = 5 messages compacted, indices [2, 7).
    auto plan = eng.plan(m, 4500, 8192);
    EXPECT_FALSE(plan.is_empty());
    EXPECT_EQ(plan.keep_head,     2u);
    EXPECT_EQ(plan.keep_tail,     3u);
    EXPECT_EQ(plan.compact_start, 2u);
    EXPECT_EQ(plan.compact_end,   7u);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, ZeroWindowReturnsEmptyAndDoesNotDivide) {
    WindowedContextEngine eng;
    auto m = messages(20, 100);
    auto plan = eng.plan(m, 999999, 0);  // window=0 — must not crash
    EXPECT_TRUE(plan.is_empty());
}

TEST(ContextEngineTest, ZeroMessagesReturnsEmpty) {
    WindowedContextEngine eng;
    auto plan = eng.plan({}, 999999, 8192);
    EXPECT_TRUE(plan.is_empty());
}

// ---------------------------------------------------------------------------
// Pure function: repeat calls yield identical results
// ---------------------------------------------------------------------------
TEST(ContextEngineTest, PlanIsStableAcrossCalls) {
    WindowedContextEngine eng;
    auto m = messages(20, 100);
    auto a = eng.plan(m, 7000, 8192);
    auto b = eng.plan(m, 7000, 8192);
    EXPECT_EQ(a.keep_head,     b.keep_head);
    EXPECT_EQ(a.keep_tail,     b.keep_tail);
    EXPECT_EQ(a.compact_start, b.compact_start);
    EXPECT_EQ(a.compact_end,   b.compact_end);
    EXPECT_EQ(a.summary_role,  b.summary_role);
}

} // namespace
} // namespace euxis::runtime
