/// @file
/// @brief Dynamic subagent delegation coordinator.
///
/// Mirrors the Hermes Agent tools/delegate_tool.py pattern: spawn one
/// or more isolated subagents at runtime, each with their own iteration
/// budget, and aggregate their results. Where the existing
/// SwarmOrchestrator runs a *static playbook graph* loaded from JSON,
/// this coordinator lets a parent agent decide at runtime ("this
/// monorepo has 12 services, audit each in parallel") and fan out.
///
/// Subagent budgets are independent of the parent's — a subagent that
/// burns its own 50-iteration cap does NOT drain the parent's 90-cap.
/// Recursive delegation is the caller's responsibility to forbid; the
/// coordinator just provides the fan-out primitive.
///
/// The SubagentFn is injected, so the coordinator has no dependency on
/// libs/runtime::AgentLoopHarness directly. Production callers will
/// supply a function that boots a harness per subagent; tests supply a
/// deterministic mock.
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::core {

/// @brief Specification of one subagent task to run.
struct DelegateRequest {
    std::string  subagent_id;          ///< Role/name for telemetry.
    std::string  task;                  ///< Free-form prompt to the subagent.

    /// @brief Per-subagent iteration cap. Defaults to the Hermes
    ///        subagent default (50); the IterationBudget header in
    ///        libs/runtime carries the canonical constant.
    std::size_t  iteration_budget_max{50};

    /// @brief Allowed-tool allowlist for the subagent. Empty = the
    ///        SubagentFn's caller-supplied default.
    std::vector<std::string> allowed_tools;

    /// @brief Caller-supplied metadata, opaque to the coordinator.
    nlohmann::json metadata{};
};

/// @brief Outcome of one subagent invocation.
struct DelegateResult {
    std::string  subagent_id;
    bool         success{false};
    std::string  output;
    std::string  error;
    std::size_t  iterations_consumed{0};
    std::chrono::milliseconds wall_time{0};
};

/// @brief Coordinator that fans out subagent invocations.
///
/// Thread-safe — concurrent calls to delegate() and delegate_parallel()
/// are safe. Statistics (total_invocations / total_iterations_consumed)
/// are kept in atomics.
class DelegationCoordinator {
public:
    /// @brief Caller-supplied function that runs one subagent.
    ///
    /// Production: build an AgentLoopHarness with the requested budget,
    /// register the allowed tools, drive the loop, return the rollup.
    /// Tests: a deterministic mock.
    using SubagentFn = std::function<DelegateResult(const DelegateRequest&)>;

    /// @brief Construct with the subagent runner. @p fn must not be null.
    explicit DelegationCoordinator(SubagentFn fn);

    /// @brief Run one subagent synchronously.
    [[nodiscard]] auto delegate(DelegateRequest req) -> DelegateResult;

    /// @brief Run @p reqs in parallel. One std::thread per request.
    ///        Returns when every subagent has completed; ordering of
    ///        the returned vector matches the input.
    [[nodiscard]] auto delegate_parallel(std::vector<DelegateRequest> reqs)
        -> std::vector<DelegateResult>;

    /// @brief Total number of subagents invoked (success + failure).
    [[nodiscard]] auto total_invocations() const noexcept -> std::size_t;

    /// @brief Sum of iterations_consumed across all completed subagents.
    [[nodiscard]] auto total_iterations_consumed() const noexcept -> std::size_t;

    /// @brief True iff the coordinator has a valid SubagentFn.
    [[nodiscard]] auto is_ready() const noexcept -> bool;

private:
    SubagentFn               fn_;
    std::atomic<std::size_t> total_invocations_{0};
    std::atomic<std::size_t> total_iterations_consumed_{0};
};

} // namespace euxis::core
