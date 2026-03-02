#include <gtest/gtest.h>

#include "euxis/core/topology_grid.hpp"

namespace euxis::core {
namespace {

TEST(TopologyGridTest, SetAndGet) {
    TopologyGrid grid(3);
    grid.set_cost(0, 1, 5.0);
    grid.set_cost(1, 2, 3.0);

    EXPECT_DOUBLE_EQ(grid.cost(0, 1), 5.0);
    EXPECT_DOUBLE_EQ(grid.cost(1, 2), 3.0);
    EXPECT_DOUBLE_EQ(grid.cost(0, 0), 0.0);  // default
}

TEST(TopologyGridTest, OutOfBoundsThrows) {
    TopologyGrid grid(2);
    EXPECT_THROW(grid.set_cost(5, 0, 1.0), std::out_of_range);
    EXPECT_THROW((void)grid.cost(0, 5), std::out_of_range);
}

TEST(TopologyGridTest, MdspanView) {
    TopologyGrid grid(2);
    grid.set_cost(0, 1, 7.0);
    grid.set_cost(1, 0, 3.0);

    auto v = grid.view();
    double v01 = v[0, 1];
    double v10 = v[1, 0];
    EXPECT_DOUBLE_EQ(v01, 7.0);
    EXPECT_DOUBLE_EQ(v10, 3.0);
}

TEST(TopologyGridTest, BestCoordinator) {
    TopologyGrid grid(3);
    // Agent 0: total outgoing = 10 + 20 = 30
    grid.set_cost(0, 1, 10.0);
    grid.set_cost(0, 2, 20.0);
    // Agent 1: total outgoing = 5 + 3 = 8
    grid.set_cost(1, 0, 5.0);
    grid.set_cost(1, 2, 3.0);
    // Agent 2: total outgoing = 15 + 12 = 27
    grid.set_cost(2, 0, 15.0);
    grid.set_cost(2, 1, 12.0);

    EXPECT_EQ(grid.best_coordinator(), 1u);
}

TEST(TopologyGridTest, Reachable) {
    TopologyGrid grid(3);
    grid.set_cost(0, 1, 5.0);
    grid.set_cost(1, 2, 3.0);

    EXPECT_TRUE(grid.reachable(0, 1));
    EXPECT_FALSE(grid.reachable(0, 2));
    EXPECT_TRUE(grid.reachable(1, 2));
}

TEST(TopologyGridTest, NumAgents) {
    TopologyGrid grid(4);
    EXPECT_EQ(grid.num_agents(), 4u);
}

// --- Coverage: line 36 (best_coordinator on empty grid) ---
TEST(TopologyGridTest, BestCoordinatorEmptyThrows) {
    TopologyGrid grid(0);
    EXPECT_THROW((void)grid.best_coordinator(), std::logic_error);
}

// --- Coverage: line 56 (reachable out of bounds) ---
TEST(TopologyGridTest, ReachableOutOfBoundsThrows) {
    TopologyGrid grid(2);
    EXPECT_THROW((void)grid.reachable(5, 0), std::out_of_range);
    EXPECT_THROW((void)grid.reachable(0, 5), std::out_of_range);
}

} // namespace
} // namespace euxis::core
