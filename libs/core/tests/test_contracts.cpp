/// @file
/// @brief Compile-time and link-time tests for the core umbrella header.
///
/// Including <euxis/core/contracts.hpp> must pull in every declared public
/// type from the libs/core SDK in one go. This test would have caught the
/// pre-existing #include "resilience.hpp" -> file-not-found bug that
/// silently survived because no production consumer included the umbrella.

#include <gtest/gtest.h>

// The whole point: a single umbrella include must compile.
#include "euxis/core/contracts.hpp"

namespace euxis::core {
namespace {

// Each test instantiates a public type pulled in transitively by contracts.hpp
// to prove the umbrella exports the intended surface.

TEST(CoreContractsTest, UmbrellaExportsRouter) {
    FinOpsRouter router{};
    EXPECT_FALSE(router.select_provider("low").empty());
}

TEST(CoreContractsTest, UmbrellaExportsSwarm) {
    SwarmOrchestrator sw{"http://localhost:8080"};
    auto history = sw.execute_playbook(
        nlohmann::json{{"name", "smoke"}, {"phases", nlohmann::json::array()}},
        "smoke-goal");
    EXPECT_TRUE(history.empty());
}

TEST(CoreContractsTest, UmbrellaExportsTypes) {
    AgentExecutionRequest req{};
    EXPECT_DOUBLE_EQ(req.timeout_seconds, 30.0);

    AgentExecutionResult res{};
    EXPECT_FALSE(res.ok);
    EXPECT_FALSE(res.error.has_value());
    EXPECT_EQ(res.latency_ms, 0);

    SwarmTask task{};
    EXPECT_EQ(task.status, "pending");
    EXPECT_EQ(task.complexity, "medium");
}

TEST(CoreContractsTest, UmbrellaExportsResilience) {
    // resilience.hpp lives in libs/network but contracts.hpp re-exports it.
    // If anyone removes the re-export, this fails to compile.
    network::CircuitBreaker cb{3, 5.0};
    EXPECT_FALSE(cb.is_open());
}

} // namespace
} // namespace euxis::core
