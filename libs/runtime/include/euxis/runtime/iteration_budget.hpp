/// @file
/// @brief Per-turn iteration budget for agent loops.
#pragma once

#include <atomic>
#include <cstddef>

namespace euxis::runtime {

/// @brief Thread-safe per-turn iteration counter for agent loops.
///
/// Mirrors the Hermes Agent `agent/iteration_budget.py` pattern: a single
/// counter bounds the number of model-call + tool-dispatch iterations
/// inside one user turn. Without a budget, a flaky tool that always
/// returns "retry" will loop forever and burn cost; with a budget the
/// loop terminates predictably.
///
/// Defaults follow Hermes conventions: parent agents get 90 iterations
/// per turn, subagents spawned via delegation get 50 (independent counters
/// — subagent budgets do not draw down the parent's). Callers can refund
/// iterations for programmatic tool use (e.g. an `execute_code` shim that
/// shouldn't count against the agentic loop limit).
class IterationBudget {
public:
    /// @brief Default parent-agent budget (matches Hermes `max_iterations`).
    static constexpr std::size_t kDefaultParent = 90;

    /// @brief Default subagent budget (matches Hermes `delegation.max_iterations`).
    static constexpr std::size_t kDefaultSubagent = 50;

    /// @brief Construct a budget with @p max iterations available.
    explicit IterationBudget(std::size_t max = kDefaultParent) noexcept
        : max_{max}, remaining_{max} {}

    /// @brief Factory for a subagent budget (independent of the parent's).
    [[nodiscard]] static auto subagent(std::size_t max = kDefaultSubagent) noexcept
        -> IterationBudget {
        return IterationBudget{max};
    }

    /// @brief Attempt to consume one iteration. Returns false when exhausted.
    [[nodiscard]] auto try_consume() noexcept -> bool {
        // Decrement only if the current value is > 0. Compare-and-swap loop
        // gives strict bounding under contention without locks.
        std::size_t current = remaining_.load(std::memory_order_acquire);
        while (current > 0) {
            if (remaining_.compare_exchange_weak(
                    current, current - 1,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
        return false;
    }

    /// @brief Refund one iteration. Used for tool calls that should not count
    ///        against the agentic-loop limit (e.g. `execute_code` shims).
    /// @return true on success, false if the budget is already full.
    auto refund() noexcept -> bool {
        std::size_t current = remaining_.load(std::memory_order_acquire);
        while (current < max_) {
            if (remaining_.compare_exchange_weak(
                    current, current + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
        return false;
    }

    /// @brief Reset the counter back to the original maximum.
    void reset() noexcept {
        remaining_.store(max_, std::memory_order_release);
    }

    /// @brief Iterations still available.
    [[nodiscard]] auto remaining() const noexcept -> std::size_t {
        return remaining_.load(std::memory_order_acquire);
    }

    /// @brief Iterations consumed so far (max - remaining).
    [[nodiscard]] auto consumed() const noexcept -> std::size_t {
        return max_ - remaining_.load(std::memory_order_acquire);
    }

    /// @brief Maximum budget configured at construction.
    [[nodiscard]] auto capacity() const noexcept -> std::size_t {
        return max_;
    }

    /// @brief True if the budget has been fully consumed.
    [[nodiscard]] auto exhausted() const noexcept -> bool {
        return remaining_.load(std::memory_order_acquire) == 0;
    }

private:
    std::size_t max_;
    std::atomic<std::size_t> remaining_;
};

} // namespace euxis::runtime
