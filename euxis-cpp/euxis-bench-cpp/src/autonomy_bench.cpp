#include "euxis/bench/runner.hpp"

#include <chrono>
#include <format>

#include "euxis/a2a/task.hpp"

#include <spdlog/spdlog.h>

namespace euxis::bench {

// ---------------------------------------------------------------------------
// Benchmark: task_lifecycle
// ---------------------------------------------------------------------------
static auto bench_task_lifecycle() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "task_lifecycle";
    result.suite = "autonomy";
    result.unit = "ops/sec";
    result.target = 100'000.0;

    // An "operation" is: create a task, transition through all valid states
    // (Pending -> Active -> Completed), constituting a full lifecycle.
    // We count each full lifecycle as one operation.

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    size_t ops = 0;
    const auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() < deadline) {
        auto task = a2a::create_task();
        auto r1 = a2a::transition_task(task, a2a::TaskStatus::Active);
        auto r2 = a2a::transition_task(task, a2a::TaskStatus::Completed);

        if (r1.has_value() && r2.has_value()) {
            ++ops;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double elapsed_sec =
        static_cast<double>(elapsed_us.count()) / 1'000'000.0;

    result.duration = elapsed_us;
    result.value = static_cast<double>(ops) / elapsed_sec;
    result.passed = result.value >= result.target;
    result.message = std::format(
        "{} lifecycle ops in {:.3f}s = {:.0f} ops/sec",
        ops, elapsed_sec, result.value);

    spdlog::info("autonomy/task_lifecycle: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Benchmark: error_recovery
// ---------------------------------------------------------------------------
static auto bench_error_recovery() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "error_recovery";
    result.suite = "autonomy";
    result.unit = "%";
    result.target = 100.0;

    const auto start = std::chrono::steady_clock::now();

    // Test invalid transitions and verify they are all correctly rejected.
    struct InvalidTransition {
        a2a::TaskStatus from;
        a2a::TaskStatus to;
    };

    // Invalid transitions per the A2A state machine:
    // Pending -> Completed is invalid
    // Completed/Failed/Cancelled -> anything is invalid (terminal states)
    const std::vector<InvalidTransition> invalid_transitions = {
        {a2a::TaskStatus::Pending,   a2a::TaskStatus::Completed},
        {a2a::TaskStatus::Completed, a2a::TaskStatus::Active},
        {a2a::TaskStatus::Completed, a2a::TaskStatus::Pending},
        {a2a::TaskStatus::Completed, a2a::TaskStatus::Failed},
        {a2a::TaskStatus::Completed, a2a::TaskStatus::Cancelled},
        {a2a::TaskStatus::Failed,    a2a::TaskStatus::Active},
        {a2a::TaskStatus::Failed,    a2a::TaskStatus::Completed},
        {a2a::TaskStatus::Failed,    a2a::TaskStatus::Cancelled},
        {a2a::TaskStatus::Cancelled, a2a::TaskStatus::Active},
        {a2a::TaskStatus::Cancelled, a2a::TaskStatus::Completed},
        {a2a::TaskStatus::Cancelled, a2a::TaskStatus::Failed},
    };

    size_t total_tests = 0;
    size_t rejected = 0;

    for (const auto& [from, to] : invalid_transitions) {
        // Create a task and bring it to the 'from' state.
        auto task = a2a::create_task();

        bool setup_ok = true;
        if (from == a2a::TaskStatus::Active) {
            setup_ok = a2a::transition_task(task, a2a::TaskStatus::Active).has_value();
        } else if (from == a2a::TaskStatus::Completed) {
            setup_ok = a2a::transition_task(task, a2a::TaskStatus::Active).has_value()
                    && a2a::transition_task(task, a2a::TaskStatus::Completed).has_value();
        } else if (from == a2a::TaskStatus::Failed) {
            setup_ok = a2a::transition_task(task, a2a::TaskStatus::Failed).has_value();
        } else if (from == a2a::TaskStatus::Cancelled) {
            setup_ok = a2a::transition_task(task, a2a::TaskStatus::Cancelled).has_value();
        }
        // Pending requires no setup.

        if (!setup_ok) {
            continue;
        }

        ++total_tests;

        // Attempt the invalid transition; it should be rejected.
        auto attempt = a2a::transition_task(task, to);
        if (!attempt.has_value()) {
            ++rejected;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start);

    result.value = total_tests > 0
        ? (static_cast<double>(rejected) / static_cast<double>(total_tests)) * 100.0
        : 0.0;
    result.passed = result.value >= result.target;
    result.message = std::format(
        "Rejected {}/{} invalid transitions ({:.0f}%)",
        rejected, total_tests, result.value);

    spdlog::info("autonomy/error_recovery: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
void register_autonomy_benchmarks(BenchmarkRunner& runner) {
    runner.register_benchmark("autonomy", "task_lifecycle",
                              bench_task_lifecycle);
    runner.register_benchmark("autonomy", "error_recovery",
                              bench_error_recovery);
}

} // namespace euxis::bench
