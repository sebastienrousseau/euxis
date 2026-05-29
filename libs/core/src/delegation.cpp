/// @file
/// @brief Implementation of the dynamic subagent delegation coordinator.

#include "euxis/core/delegation.hpp"

#include <chrono>
#include <future>
#include <thread>
#include <utility>
#include <vector>

namespace euxis::core {

DelegationCoordinator::DelegationCoordinator(SubagentFn fn)
    : fn_{std::move(fn)} {}

auto DelegationCoordinator::delegate(DelegateRequest req) -> DelegateResult {
    DelegateResult result;
    result.subagent_id = req.subagent_id;

    if (!fn_) {
        result.error = "no subagent function configured";
        return result;
    }

    const auto t0 = std::chrono::steady_clock::now();
    result = fn_(req);
    const auto t1 = std::chrono::steady_clock::now();

    // The SubagentFn may set its own wall_time; if it did not, fall back
    // to the coordinator's measurement.
    if (result.wall_time.count() == 0) {
        result.wall_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    }
    if (result.subagent_id.empty()) result.subagent_id = req.subagent_id;

    total_invocations_.fetch_add(1, std::memory_order_relaxed);
    total_iterations_consumed_.fetch_add(result.iterations_consumed,
                                          std::memory_order_relaxed);
    return result;
}

auto DelegationCoordinator::delegate_parallel(std::vector<DelegateRequest> reqs)
    -> std::vector<DelegateResult> {
    const std::size_t n = reqs.size();
    std::vector<DelegateResult> results(n);

    if (n == 0) return results;
    if (!fn_) {
        for (std::size_t i = 0; i < n; ++i) {
            results[i].subagent_id = reqs[i].subagent_id;
            results[i].error       = "no subagent function configured";
        }
        return results;
    }

    // One thread per request — appropriate for audit fan-out which is
    // coarse-grained (typically single digits). A bounded thread pool
    // would be a future cycle when the typical fan-out grows.
    std::vector<std::thread> workers;
    workers.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        workers.emplace_back([this, &results, i, req = std::move(reqs[i])]() {
            results[i] = this->delegate(req);
        });
    }
    for (auto& w : workers) w.join();
    return results;
}

auto DelegationCoordinator::total_invocations() const noexcept -> std::size_t {
    return total_invocations_.load(std::memory_order_acquire);
}

auto DelegationCoordinator::total_iterations_consumed() const noexcept
    -> std::size_t {
    return total_iterations_consumed_.load(std::memory_order_acquire);
}

auto DelegationCoordinator::is_ready() const noexcept -> bool {
    return static_cast<bool>(fn_);
}

} // namespace euxis::core
