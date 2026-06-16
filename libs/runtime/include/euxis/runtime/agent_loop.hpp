/// @file
/// @brief AgentLoopHarness — single-turn agent-loop bundle.
///
/// Closes the gap identified by the deep-read survey ("apps/cli does not
/// use libs/runtime — advanced features stay theoretical"). The harness
/// bundles the four loop primitives shipped in Tier 1 + Tier 2 so a CLI
/// command or gateway endpoint can drive a model turn with bounded
/// iteration, context compaction, and conversation persistence without
/// hand-rolling the wiring.
///
/// Composes:
///   - IterationBudget          (libs/runtime/iteration_budget.hpp)
///   - IContextEngine + default (libs/runtime/context_engine.hpp)
///   - Conversation messages    (libs/runtime/agent_session.hpp)
///   - Provider response shape  (libs/runtime/provider.hpp)
///
/// Deliberately does NOT depend on:
///   - libs/metrics             (caller wires UsageRecord on the outside)
///   - libs/core::CredentialPool (caller manages the pool)
///   - libs/inference engines   (provider call is a std::function injected
///                               by the caller — testable with a mock)
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "agent_session.hpp"
#include "context_engine.hpp"
#include "iteration_budget.hpp"
#include "provider.hpp"

namespace euxis::runtime {

/// @brief Outcome of a single AgentLoopHarness turn.
struct TurnResult {
    ProviderResponse provider_response{};
    /// @brief True when an iteration was successfully consumed before
    ///        the provider call ran.
    bool budget_consumed{false};
    /// @brief True when the context engine recommended compaction
    ///        before this turn. The harness does NOT compact for the
    ///        caller — it surfaces the plan so the caller can decide
    ///        what summarisation cost they want to pay.
    bool compaction_recommended{false};
    CompactionPlan compaction_plan{};
    std::size_t messages_after{0};
};

/// @brief Bundle of the loop primitives needed to drive one agent turn.
///
/// Owns the conversation message vector and the iteration budget. The
/// context engine is held by unique_ptr so callers can swap in custom
/// strategies. The harness has no dependency on a specific provider —
/// run_turn() takes a std::function the caller provides.
class AgentLoopHarness {
public:
    struct Config {
        std::string session_id;
        std::string agent_id;

        /// @brief Maximum iterations consumable in this loop.
        std::size_t iteration_budget_max{IterationBudget::kDefaultParent};

        /// @brief Context window size for the target model (tokens).
        std::size_t context_window{200'000};

        /// @brief Custom context engine; default is WindowedContextEngine.
        std::unique_ptr<IContextEngine> context_engine{};
    };

    AgentLoopHarness();
    explicit AgentLoopHarness(Config cfg);

    AgentLoopHarness(const AgentLoopHarness&)            = delete;
    AgentLoopHarness& operator=(const AgentLoopHarness&) = delete;
    // Move would be a default-deletion: IterationBudget holds a
    // std::atomic, which is non-movable. The harness is constructed
    // in-place at every observed call site (test_agent_loop.cpp);
    // delete move so the compiler stops trying to synthesise a
    // broken one.
    AgentLoopHarness(AgentLoopHarness&&)            = delete;
    AgentLoopHarness& operator=(AgentLoopHarness&&) = delete;

    /// @brief Append a message to the conversation history.
    void add_message(ConversationMessage msg);

    /// @brief Current conversation messages.
    [[nodiscard]] auto messages() const noexcept
        -> const std::vector<ConversationMessage>&;

    /// @brief Iteration budget (read-only access).
    [[nodiscard]] auto budget() const noexcept -> const IterationBudget&;

    /// @brief Try to consume one iteration. False when exhausted.
    [[nodiscard]] auto try_consume_budget() noexcept -> bool;

    /// @brief Ask the context engine whether compaction is recommended now.
    ///
    /// Pure query — does not mutate state. Returns an empty CompactionPlan
    /// when below the engine's trigger threshold.
    [[nodiscard]] auto check_compaction() const -> CompactionPlan;

    /// @brief Run one full turn: budget check → compaction query →
    ///        provider call → message append.
    ///
    /// @param provider_call Caller-supplied function returning a
    ///                      ProviderResponse. The harness does NOT
    ///                      build the prompt — the caller does.
    /// @return TurnResult with diagnostics. If the budget is exhausted
    ///         the provider call is NOT invoked; provider_response
    ///         carries success=false and error="budget exhausted".
    [[nodiscard]] auto run_turn(
        std::function<ProviderResponse()> provider_call) -> TurnResult;

    [[nodiscard]] auto session_id() const noexcept -> const std::string&;
    [[nodiscard]] auto agent_id()   const noexcept -> const std::string&;
    [[nodiscard]] auto context_window() const noexcept -> std::size_t;

private:
    Config                          cfg_;
    std::unique_ptr<IContextEngine> engine_;
    IterationBudget                 budget_;
    std::vector<ConversationMessage> messages_;
};

} // namespace euxis::runtime
