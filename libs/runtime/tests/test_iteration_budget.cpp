/// @file
/// @brief Tests for the per-turn iteration budget.

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "euxis/runtime/iteration_budget.hpp"

namespace euxis::runtime {
namespace {

// ---------------------------------------------------------------------------
// Defaults follow the Hermes parent/subagent split
// ---------------------------------------------------------------------------
TEST(IterationBudgetTest, DefaultParentBudgetIsNinety) {
    IterationBudget b;
    EXPECT_EQ(b.capacity(),  90u);
    EXPECT_EQ(b.remaining(), 90u);
    EXPECT_EQ(b.consumed(),  0u);
    EXPECT_FALSE(b.exhausted());
}

TEST(IterationBudgetTest, SubagentDefaultBudgetIsFifty) {
    auto b = IterationBudget::subagent();
    EXPECT_EQ(b.capacity(),  50u);
    EXPECT_EQ(b.remaining(), 50u);
}

TEST(IterationBudgetTest, CustomBudget) {
    IterationBudget b{5};
    EXPECT_EQ(b.capacity(),  5u);
    EXPECT_EQ(b.remaining(), 5u);
}

// ---------------------------------------------------------------------------
// Basic consume → exhaustion semantics
// ---------------------------------------------------------------------------
TEST(IterationBudgetTest, ConsumeDecrementsRemaining) {
    IterationBudget b{3};
    EXPECT_TRUE(b.try_consume());
    EXPECT_EQ(b.remaining(), 2u);
    EXPECT_EQ(b.consumed(),  1u);

    EXPECT_TRUE(b.try_consume());
    EXPECT_TRUE(b.try_consume());
    EXPECT_EQ(b.remaining(), 0u);
    EXPECT_TRUE(b.exhausted());
}

TEST(IterationBudgetTest, ConsumePastExhaustionReturnsFalse) {
    IterationBudget b{1};
    EXPECT_TRUE(b.try_consume());
    EXPECT_FALSE(b.try_consume());
    EXPECT_FALSE(b.try_consume());
    EXPECT_EQ(b.remaining(), 0u);
    EXPECT_EQ(b.consumed(),  1u);
}

TEST(IterationBudgetTest, ZeroBudgetExhaustsImmediately) {
    IterationBudget b{0};
    EXPECT_TRUE(b.exhausted());
    EXPECT_FALSE(b.try_consume());
}

// ---------------------------------------------------------------------------
// Refund restores iterations but never exceeds capacity
// ---------------------------------------------------------------------------
TEST(IterationBudgetTest, RefundRestoresIteration) {
    IterationBudget b{3};
    EXPECT_TRUE(b.try_consume());
    EXPECT_EQ(b.remaining(), 2u);

    EXPECT_TRUE(b.refund());
    EXPECT_EQ(b.remaining(), 3u);
}

TEST(IterationBudgetTest, RefundCannotExceedCapacity) {
    IterationBudget b{2};
    EXPECT_FALSE(b.refund()) << "full budget cannot be refunded";
    EXPECT_EQ(b.remaining(), 2u);

    EXPECT_TRUE(b.try_consume());
    EXPECT_TRUE(b.refund());
    EXPECT_FALSE(b.refund()) << "still at capacity";
}

// ---------------------------------------------------------------------------
// Reset returns to the original capacity
// ---------------------------------------------------------------------------
TEST(IterationBudgetTest, ResetRestoresCapacity) {
    IterationBudget b{5};
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(b.try_consume());
    }
    EXPECT_TRUE(b.exhausted());

    b.reset();
    EXPECT_EQ(b.remaining(), 5u);
    EXPECT_FALSE(b.exhausted());
}

// ---------------------------------------------------------------------------
// Concurrent consumers never over-deplete
// ---------------------------------------------------------------------------
TEST(IterationBudgetTest, ConcurrentConsumersHonourCap) {
    constexpr std::size_t kBudget  = 1000;
    constexpr std::size_t kThreads = 8;
    constexpr std::size_t kTriesPerThread = 500;

    IterationBudget b{kBudget};
    std::atomic<std::size_t> successes{0};

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (std::size_t t = 0; t < kThreads; ++t) {
        workers.emplace_back([&] {
            for (std::size_t i = 0; i < kTriesPerThread; ++i) {
                if (b.try_consume()) {
                    successes.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& w : workers) w.join();

    // Total attempts (8 * 500 = 4000) far exceed the budget (1000), but the
    // atomic compare-and-swap loop guarantees we never exceed capacity.
    EXPECT_EQ(successes.load(), kBudget);
    EXPECT_EQ(b.remaining(),    0u);
    EXPECT_TRUE(b.exhausted());
}

// ---------------------------------------------------------------------------
// Subagents are independent — parent budget unaffected by child consumption
// ---------------------------------------------------------------------------
TEST(IterationBudgetTest, SubagentBudgetIsIndependentOfParent) {
    IterationBudget parent;
    auto child = IterationBudget::subagent();

    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(child.try_consume());
    }
    EXPECT_TRUE(child.exhausted());
    EXPECT_EQ(parent.remaining(), 90u) << "parent is untouched";
}

} // namespace
} // namespace euxis::runtime
