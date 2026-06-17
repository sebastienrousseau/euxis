/// @file
/// @brief Tests for the dynamic subagent delegation coordinator.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "euxis/core/delegation.hpp"

namespace euxis::core {
namespace {

// Build a deterministic mock subagent that succeeds and reports the
// supplied iteration count. wall_time is left at default so the
// coordinator's measurement is used.
auto make_ok_subagent(std::size_t iterations) -> DelegationCoordinator::SubagentFn {
    return [iterations](const DelegateRequest& req) {
        DelegateResult r;
        r.subagent_id         = req.subagent_id;
        r.success             = true;
        r.output              = "ok:" + req.task;
        r.iterations_consumed = iterations;
        return r;
    };
}

// ---------------------------------------------------------------------------
// Construction / readiness
// ---------------------------------------------------------------------------
TEST(DelegationCoordinatorTest, IsReadyWhenFnSupplied) {
    DelegationCoordinator c{make_ok_subagent(1)};
    EXPECT_TRUE(c.is_ready());
}

TEST(DelegationCoordinatorTest, IsNotReadyWhenFnIsNull) {
    DelegationCoordinator c{nullptr};
    EXPECT_FALSE(c.is_ready());
}

// ---------------------------------------------------------------------------
// Single delegate
// ---------------------------------------------------------------------------
TEST(DelegationCoordinatorTest, DelegateCallsFnAndCarriesResult) {
    DelegationCoordinator c{make_ok_subagent(7)};
    DelegateRequest req;
    req.subagent_id          = "auditor-1";
    req.task                 = "scan service A";
    req.iteration_budget_max = 50;

    auto r = c.delegate(req);
    EXPECT_TRUE(r.success);
    EXPECT_EQ(r.subagent_id,            "auditor-1");
    EXPECT_EQ(r.output,                 "ok:scan service A");
    EXPECT_EQ(r.iterations_consumed,    7u);
    EXPECT_EQ(c.total_invocations(),    1u);
    EXPECT_EQ(c.total_iterations_consumed(), 7u);
}

TEST(DelegationCoordinatorTest, MissingFnReturnsErrorAndDoesNotCount) {
    DelegationCoordinator c{nullptr};
    DelegateRequest req;
    req.subagent_id = "auditor-1";
    auto r = c.delegate(std::move(req));
    EXPECT_FALSE(r.success);
    EXPECT_EQ(r.error, "no subagent function configured");
    // No SubagentFn ran — no stats accrued.
    EXPECT_EQ(c.total_invocations(),           0u);
    EXPECT_EQ(c.total_iterations_consumed(),   0u);
}

TEST(DelegationCoordinatorTest, WallTimeFallbackIsMeasured) {
    // SubagentFn that does not set wall_time.
    auto fn = [](const DelegateRequest& req) {
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        DelegateResult r;
        r.subagent_id = req.subagent_id;
        r.success     = true;
        return r;
    };
    DelegationCoordinator c{fn};
    DelegateRequest req;
    req.subagent_id = "x";
    auto r = c.delegate(req);
    EXPECT_TRUE(r.success);
    EXPECT_GE(r.wall_time.count(), 15) << "coordinator must measure when fn doesn't";
}

// ---------------------------------------------------------------------------
// Parallel fan-out
// ---------------------------------------------------------------------------
TEST(DelegationCoordinatorTest, ParallelOrderingMatchesInput) {
    DelegationCoordinator c{make_ok_subagent(2)};
    std::vector<DelegateRequest> reqs;
    for (int i = 0; i < 5; ++i) {
        DelegateRequest r;
        r.subagent_id = "auditor-" + std::to_string(i);
        r.task        = "task-" + std::to_string(i);
        reqs.push_back(std::move(r));
    }
    auto results = c.delegate_parallel(reqs);
    ASSERT_EQ(results.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(results[static_cast<std::size_t>(i)].subagent_id,
                  "auditor-" + std::to_string(i));
        EXPECT_EQ(results[static_cast<std::size_t>(i)].output,
                  "ok:task-" + std::to_string(i));
    }
}

TEST(DelegationCoordinatorTest, ParallelAccumulatesStatsCorrectly) {
    DelegationCoordinator c{make_ok_subagent(3)};
    std::vector<DelegateRequest> reqs;
    for (int i = 0; i < 4; ++i) {
        DelegateRequest r;
        r.subagent_id = "auditor-" + std::to_string(i);
        reqs.push_back(std::move(r));
    }
    auto results = c.delegate_parallel(std::move(reqs));
    EXPECT_EQ(results.size(), 4u);
    EXPECT_EQ(c.total_invocations(),           4u);
    EXPECT_EQ(c.total_iterations_consumed(), 12u) << "3 iters * 4 subagents";
}

TEST(DelegationCoordinatorTest, ParallelMissingFnReturnsErrors) {
    DelegationCoordinator c{nullptr};
    std::vector<DelegateRequest> reqs(3);
    for (std::size_t i = 0; i < 3; ++i) {
        reqs[i].subagent_id = "x" + std::to_string(i);
    }
    auto results = c.delegate_parallel(reqs);
    ASSERT_EQ(results.size(), 3u);
    for (const auto& r : results) {
        EXPECT_FALSE(r.success);
        EXPECT_EQ(r.error, "no subagent function configured");
    }
    EXPECT_EQ(c.total_invocations(), 0u);
}

TEST(DelegationCoordinatorTest, EmptyRequestVectorReturnsEmpty) {
    DelegationCoordinator c{make_ok_subagent(1)};
    auto results = c.delegate_parallel({});
    EXPECT_TRUE(results.empty());
    EXPECT_EQ(c.total_invocations(), 0u);
}

// ---------------------------------------------------------------------------
// Parallel execution is genuinely concurrent
// ---------------------------------------------------------------------------
TEST(DelegationCoordinatorTest, ParallelExecutesConcurrently) {
    // Each subagent sleeps 100 ms. 4 subagents in parallel should
    // complete in roughly 100-200 ms wall time, not 400 ms.
    auto fn = [](const DelegateRequest& req) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        DelegateResult r;
        r.subagent_id         = req.subagent_id;
        r.success             = true;
        r.iterations_consumed = 1;
        return r;
    };
    DelegationCoordinator c{fn};
    std::vector<DelegateRequest> reqs(4);
    for (std::size_t i = 0; i < 4; ++i) {
        reqs[i].subagent_id = "x" + std::to_string(i);
    }

    const auto t0 = std::chrono::steady_clock::now();
    auto results = c.delegate_parallel(std::move(reqs));
    const auto t1 = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    EXPECT_EQ(results.size(), 4u);
    EXPECT_LT(elapsed_ms.count(), 350)
        << "parallel fan-out must beat sequential (~400 ms)";
}

// ---------------------------------------------------------------------------
// Subagent ids preserved when SubagentFn forgets to set them
// ---------------------------------------------------------------------------
TEST(DelegationCoordinatorTest, SubagentIdFalledBackToRequest) {
    // Fn forgets to populate subagent_id.
    auto fn = [](const DelegateRequest& /*req*/) {
        DelegateResult r;
        r.success = true;
        return r;
    };
    DelegationCoordinator c{fn};
    DelegateRequest req;
    req.subagent_id = "forgotten-id";
    auto r = c.delegate(req);
    EXPECT_EQ(r.subagent_id, "forgotten-id");
}

} // namespace
} // namespace euxis::core
