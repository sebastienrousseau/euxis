/// @file
/// @brief Implementation of the windowed context-compaction strategy.

#include "euxis/runtime/context_engine.hpp"

#include <algorithm>
#include <cstddef>

#include "euxis/runtime/contracts.hpp"

namespace euxis::runtime {

WindowedContextEngine::WindowedContextEngine() noexcept
    : cfg_{} {}

WindowedContextEngine::WindowedContextEngine(Config cfg) noexcept
    : cfg_{cfg} {}

auto WindowedContextEngine::plan(
    const std::vector<ConversationMessage>& messages,
    std::size_t estimated_tokens,
    std::size_t context_window) const -> CompactionPlan {

    CompactionPlan empty{};
    empty.summary_role = "system";

    // No context window → cannot reason about trigger threshold. Bail out
    // before any division. Callers that don't know the window should pass
    // 0; this returns an empty plan rather than crashing.
    if (context_window == 0) return empty;

    // Below trigger threshold → no compaction. Use integer arithmetic to
    // avoid surprising rounding; threshold = window * ratio.
    const double threshold_d =
        static_cast<double>(context_window) * cfg_.trigger_ratio;
    const std::size_t threshold =
        threshold_d < 0.0
            ? 0u
            : static_cast<std::size_t>(threshold_d);
    if (estimated_tokens < threshold) return empty;

    // Need a middle to compact: total > head + tail. If not, the plan is
    // empty even when above threshold (already as small as it gets).
    const std::size_t n = messages.size();
    const std::size_t head = cfg_.keep_head_messages;
    const std::size_t tail = cfg_.keep_tail_messages;
    if (n <= head + tail) return empty;

    CompactionPlan plan;
    plan.keep_head     = head;
    plan.keep_tail     = tail;
    plan.compact_start = head;
    plan.compact_end   = n - tail;
    plan.summary_role  = "system";
    // Postcondition: when plan is non-empty the head/middle/tail
    // partition is exhaustive over `messages`. Under the pilot flag
    // (EUXIS_ENABLE_CXX26_CONTRACTS=ON) a future refactor that breaks
    // this trips the runtime check; under defaults it is documentation.
    EUXIS_POST(plan.keep_head + plan.compact_count() + plan.keep_tail == n);
    return plan;
}

auto estimate_tokens(const std::vector<ConversationMessage>& messages) noexcept
    -> std::size_t {
    // 4 bytes per token rule of thumb. Sum content lengths; ignore
    // per-message metadata which is bounded and not the cost driver.
    std::size_t bytes = 0;
    for (const auto& m : messages) {
        bytes += m.content.size();
    }
    return bytes / 4;
}

} // namespace euxis::runtime
