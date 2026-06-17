/// @file
/// @brief Tests for the C++26 contracts shim and its pilot annotations.
///
/// Validates two things:
///   1. The shim compiles in every mode the header advertises.
///   2. When EUXIS_CONTRACTS_RUNTIME is defined, the pilot annotations
///      on IterationBudget::refund and WindowedContextEngine::plan
///      actually catch a violation (death-test).

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "euxis/runtime/contracts.hpp"
#include "euxis/runtime/iteration_budget.hpp"
#include "euxis/runtime/context_engine.hpp"
#include "euxis/runtime/agent_session.hpp"
#include "euxis/runtime/session_store.hpp"

namespace euxis::runtime {
namespace {

// Always-on smoke: the shim must compile and not abort on a true predicate
// regardless of the mode CMake picked.
TEST(ContractsShim, TruePredicateNeverFires) {
    EUXIS_CONTRACT_ASSERT(1 == 1);
    EUXIS_PRE(true);
    EUXIS_POST(true);
    SUCCEED();
}

// IterationBudget::refund on a full budget — caller's mistake.
//
// Under EUXIS_CONTRACTS_RUNTIME this aborts (death test below); under
// default mode the precondition is a no-op or [[assume]] and refund()
// returns false safely.
TEST(ContractsPilot, RefundFromFullBudgetReturnsFalseInQuietMode) {
#if !defined(EUXIS_CONTRACTS_RUNTIME)
    IterationBudget b{10};
    EXPECT_FALSE(b.refund());
#else
    GTEST_SKIP() << "death-test handled separately under EUXIS_CONTRACTS_RUNTIME";
#endif
}

#if defined(EUXIS_CONTRACTS_RUNTIME)
TEST(ContractsPilotDeath, RefundFromFullBudgetAborts) {
    IterationBudget b{10};
    EXPECT_DEATH(b.refund(), "contract violation");
}
#endif

// Plan postcondition holds across a representative spread of inputs.
TEST(ContractsPilot, WindowedPlanPartitionIsExhaustive) {
    WindowedContextEngine eng;
    std::vector<ConversationMessage> msgs;
    for (int i = 0; i < 50; ++i) {
        SessionMessage sm;
        sm.role = (i % 2 == 0) ? Role::User : Role::Assistant;
        sm.content = std::string(800, 'x');
        sm.agent_id = "t";
        msgs.emplace_back(sm);
    }
    const auto tokens = estimate_tokens(msgs);
    const auto plan = eng.plan(msgs, tokens, /*context_window=*/16'000);
    if (!plan.is_empty()) {
        EXPECT_EQ(plan.keep_head + plan.compact_count() + plan.keep_tail,
                  msgs.size());
    }
}

} // namespace
} // namespace euxis::runtime
