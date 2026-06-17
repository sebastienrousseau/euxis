/// @file
/// @brief Tests for the AgentLoopHarness.

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "euxis/runtime/agent_loop.hpp"
#include "euxis/runtime/context_engine.hpp"
#include "euxis/runtime/session_store.hpp"

namespace euxis::runtime {
namespace {

auto msg(Role role, std::string content) -> ConversationMessage {
    SessionMessage sm{};
    sm.role     = role;
    sm.content  = std::move(content);
    sm.agent_id = "test";
    return ConversationMessage{sm};
}

auto mock_response(std::string output, int input_tokens = 100,
                   int output_tokens = 50) {
    return [output, input_tokens, output_tokens] {
        ProviderResponse r;
        r.success       = true;
        r.output        = output;
        r.input_tokens  = input_tokens;
        r.output_tokens = output_tokens;
        r.duration_ms   = 12.5;
        return r;
    };
}

// ---------------------------------------------------------------------------
// Construction defaults
// ---------------------------------------------------------------------------
TEST(AgentLoopHarnessTest, DefaultConfigMatchesPrimitives) {
    AgentLoopHarness h;
    EXPECT_EQ(h.budget().capacity(),  IterationBudget::kDefaultParent);
    EXPECT_EQ(h.budget().remaining(), IterationBudget::kDefaultParent);
    EXPECT_EQ(h.messages().size(),    0u);
    EXPECT_EQ(h.context_window(),     200'000u);
    EXPECT_TRUE(h.session_id().empty());
    EXPECT_TRUE(h.agent_id().empty());
}

TEST(AgentLoopHarnessTest, CustomConfigPropagates) {
    AgentLoopHarness::Config cfg;
    cfg.session_id           = "s-42";
    cfg.agent_id             = "architect";
    cfg.iteration_budget_max = 7;
    cfg.context_window       = 8192;
    AgentLoopHarness h{std::move(cfg)};
    EXPECT_EQ(h.session_id(), "s-42");
    EXPECT_EQ(h.agent_id(),   "architect");
    EXPECT_EQ(h.budget().capacity(),  7u);
    EXPECT_EQ(h.context_window(),     8192u);
}

// ---------------------------------------------------------------------------
// Message append
// ---------------------------------------------------------------------------
TEST(AgentLoopHarnessTest, AddMessageAppends) {
    AgentLoopHarness h;
    EXPECT_EQ(h.messages().size(), 0u);
    h.add_message(msg(Role::System, "you are an audit agent"));
    h.add_message(msg(Role::User,   "scan ."));
    ASSERT_EQ(h.messages().size(), 2u);
    EXPECT_EQ(h.messages()[0].role, Role::System);
    EXPECT_EQ(h.messages()[1].role, Role::User);
}

// ---------------------------------------------------------------------------
// Budget consumption
// ---------------------------------------------------------------------------
TEST(AgentLoopHarnessTest, BudgetIsConsumedOnEachTurn) {
    AgentLoopHarness::Config cfg;
    cfg.iteration_budget_max = 3;
    AgentLoopHarness h{std::move(cfg)};

    for (int i = 0; i < 3; ++i) {
        auto res = h.run_turn(mock_response("ok"));
        EXPECT_TRUE(res.budget_consumed);
        EXPECT_TRUE(res.provider_response.success);
    }
    EXPECT_TRUE(h.budget().exhausted());
}

TEST(AgentLoopHarnessTest, ExhaustedBudgetSkipsProviderCall) {
    AgentLoopHarness::Config cfg;
    cfg.iteration_budget_max = 1;
    AgentLoopHarness h{std::move(cfg)};

    int call_count = 0;
    auto counting_call = [&]() -> ProviderResponse {
        ++call_count;
        ProviderResponse r{};
        r.success = true;
        return r;
    };

    auto first  = h.run_turn(counting_call);
    auto second = h.run_turn(counting_call);

    EXPECT_TRUE(first.budget_consumed);
    EXPECT_FALSE(second.budget_consumed);
    EXPECT_FALSE(second.provider_response.success);
    EXPECT_EQ(second.provider_response.error, "budget exhausted");
    EXPECT_EQ(call_count, 1) << "second turn must not invoke provider";
}

// ---------------------------------------------------------------------------
// Missing provider call yields explicit error, not crash
// ---------------------------------------------------------------------------
TEST(AgentLoopHarnessTest, NullProviderCallReportsErrorClean) {
    AgentLoopHarness h;
    auto res = h.run_turn(nullptr);
    EXPECT_TRUE(res.budget_consumed) << "still consumes one slot";
    EXPECT_FALSE(res.provider_response.success);
    EXPECT_EQ(res.provider_response.error, "no provider_call supplied");
}

// ---------------------------------------------------------------------------
// Compaction recommendation surfaces but does not mutate messages
// ---------------------------------------------------------------------------
TEST(AgentLoopHarnessTest, CompactionRecommendedAboveThreshold) {
    AgentLoopHarness::Config cfg;
    cfg.context_window = 1024;  // tight, easy to trip the 75% trigger
    cfg.iteration_budget_max = 5;
    AgentLoopHarness h{std::move(cfg)};

    // Stack 20 messages of 400 bytes each → 8000 bytes → ~2000 tokens,
    // well above 75% * 1024 = 768.
    for (int i = 0; i < 20; ++i) {
        h.add_message(msg(Role::User, std::string(400, 'x')));
    }

    auto res = h.run_turn(mock_response("ok"));
    EXPECT_TRUE(res.budget_consumed);
    EXPECT_TRUE(res.compaction_recommended);
    EXPECT_FALSE(res.compaction_plan.is_empty());
    // Harness does not run the compaction itself.
    EXPECT_EQ(h.messages().size(), 20u);
}

TEST(AgentLoopHarnessTest, CompactionNotRecommendedBelowThreshold) {
    AgentLoopHarness::Config cfg;
    cfg.context_window = 200'000;
    cfg.iteration_budget_max = 5;
    AgentLoopHarness h{std::move(cfg)};

    h.add_message(msg(Role::User, "small prompt"));
    auto res = h.run_turn(mock_response("ok"));
    EXPECT_FALSE(res.compaction_recommended);
    EXPECT_TRUE(res.compaction_plan.is_empty());
}

// ---------------------------------------------------------------------------
// run_turn carries the provider response through unchanged
// ---------------------------------------------------------------------------
TEST(AgentLoopHarnessTest, RunTurnPassesProviderResponseThrough) {
    AgentLoopHarness h;
    auto res = h.run_turn(mock_response("synthesised", 250, 75));
    EXPECT_TRUE(res.provider_response.success);
    EXPECT_EQ(res.provider_response.output,        "synthesised");
    EXPECT_EQ(res.provider_response.input_tokens,  250);
    EXPECT_EQ(res.provider_response.output_tokens, 75);
}

// ---------------------------------------------------------------------------
// Custom context engine is honoured
// ---------------------------------------------------------------------------
namespace {

struct AlwaysCompactEngine final : public IContextEngine {
    auto plan(const std::vector<ConversationMessage>& msgs,
              std::size_t /*estimated_tokens*/,
              std::size_t /*context_window*/) const -> CompactionPlan override {
        CompactionPlan p;
        p.keep_head     = 0;
        p.keep_tail     = 0;
        p.compact_start = 0;
        p.compact_end   = msgs.size();
        p.summary_role  = "system";
        return p;
    }
};

} // namespace

TEST(AgentLoopHarnessTest, CustomContextEngineIsHonoured) {
    AgentLoopHarness::Config cfg;
    cfg.context_engine       = std::make_unique<AlwaysCompactEngine>();
    cfg.iteration_budget_max = 1;
    AgentLoopHarness h{std::move(cfg)};

    h.add_message(msg(Role::User, "anything"));
    auto res = h.run_turn(mock_response("ok"));

    EXPECT_TRUE(res.compaction_recommended);
    EXPECT_EQ(res.compaction_plan.compact_count(), 1u);
}

// ---------------------------------------------------------------------------
// messages_after is captured accurately
// ---------------------------------------------------------------------------
TEST(AgentLoopHarnessTest, MessagesAfterReflectsConversationLength) {
    AgentLoopHarness h;
    h.add_message(msg(Role::User, "hello"));
    h.add_message(msg(Role::Assistant, "world"));
    auto res = h.run_turn(mock_response("ok"));
    EXPECT_EQ(res.messages_after, 2u);
}

} // namespace
} // namespace euxis::runtime
