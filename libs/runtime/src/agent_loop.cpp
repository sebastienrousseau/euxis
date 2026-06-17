/// @file
/// @brief Implementation of the AgentLoopHarness.

#include "euxis/runtime/agent_loop.hpp"

#include <memory>
#include <utility>

namespace euxis::runtime {

namespace {

/// @brief Build a fresh default engine when the caller didn't supply one.
[[nodiscard]] auto make_default_engine() -> std::unique_ptr<IContextEngine> {
    return std::make_unique<WindowedContextEngine>();
}

} // namespace

AgentLoopHarness::AgentLoopHarness()
    : cfg_{},
      engine_{make_default_engine()},
      budget_{IterationBudget::kDefaultParent} {}

AgentLoopHarness::AgentLoopHarness(Config cfg)
    : cfg_{std::move(cfg)},
      engine_{cfg_.context_engine ? std::move(cfg_.context_engine)
                                   : make_default_engine()},
      budget_{cfg_.iteration_budget_max} {}

void AgentLoopHarness::add_message(ConversationMessage msg) {
    messages_.push_back(std::move(msg));
}

auto AgentLoopHarness::messages() const noexcept
    -> const std::vector<ConversationMessage>& {
    return messages_;
}

auto AgentLoopHarness::budget() const noexcept -> const IterationBudget& {
    return budget_;
}

auto AgentLoopHarness::try_consume_budget() noexcept -> bool {
    return budget_.try_consume();
}

auto AgentLoopHarness::check_compaction() const -> CompactionPlan {
    const std::size_t tokens = estimate_tokens(messages_);
    return engine_->plan(messages_, tokens, cfg_.context_window);
}

auto AgentLoopHarness::run_turn(std::function<ProviderResponse()> provider_call)
    -> TurnResult {
    TurnResult result;

    if (!try_consume_budget()) {
        result.provider_response.success = false;
        result.provider_response.error   = "budget exhausted";
        result.budget_consumed           = false;
        result.messages_after            = messages_.size();
        return result;
    }
    result.budget_consumed = true;

    // Surface compaction recommendation BEFORE the provider call so the
    // caller can decide whether to summarise first. We do not act on the
    // plan here — that's the caller's responsibility (it costs auxiliary
    // model tokens and the harness has no provider to bill).
    auto plan = check_compaction();
    if (!plan.is_empty()) {
        result.compaction_recommended = true;
        result.compaction_plan        = std::move(plan);
    }

    // Invoke the provider. The function is allowed to be a mock for
    // testing or a real LLM call in production.
    if (provider_call) {
        result.provider_response = provider_call();
    } else {
        result.provider_response.success = false;
        result.provider_response.error   = "no provider_call supplied";
    }

    result.messages_after = messages_.size();
    return result;
}

auto AgentLoopHarness::session_id() const noexcept -> const std::string& {
    return cfg_.session_id;
}

auto AgentLoopHarness::agent_id() const noexcept -> const std::string& {
    return cfg_.agent_id;
}

auto AgentLoopHarness::context_window() const noexcept -> std::size_t {
    return cfg_.context_window;
}

} // namespace euxis::runtime
