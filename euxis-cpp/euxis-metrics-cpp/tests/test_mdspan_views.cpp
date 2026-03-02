#include <gtest/gtest.h>

#include "euxis/metrics/mdspan_views.hpp"

namespace euxis::metrics {
namespace {

TEST(AgentMetricsGridTest, SetAndGet) {
    AgentMetricsGrid grid(3, 4);
    grid.set(0, 0, 1.0);
    grid.set(1, 2, 5.5);
    grid.set(2, 3, 9.9);

    EXPECT_DOUBLE_EQ(grid.get(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(grid.get(1, 2), 5.5);
    EXPECT_DOUBLE_EQ(grid.get(2, 3), 9.9);
    EXPECT_DOUBLE_EQ(grid.get(0, 1), 0.0);  // default
}

TEST(AgentMetricsGridTest, OutOfBoundsThrows) {
    AgentMetricsGrid grid(2, 3);
    EXPECT_THROW(grid.set(5, 0, 1.0), std::out_of_range);
    EXPECT_THROW((void)grid.get(0, 10), std::out_of_range);
}

TEST(AgentMetricsGridTest, MdspanViewAccess) {
    AgentMetricsGrid grid(2, 3);
    grid.set(0, 0, 1.0);
    grid.set(0, 1, 2.0);
    grid.set(0, 2, 3.0);
    grid.set(1, 0, 4.0);
    grid.set(1, 1, 5.0);
    grid.set(1, 2, 6.0);

    auto v = grid.view();
    // Assign to temp to avoid macro comma confusion with v[r, c]
    double v00 = v[0, 0];
    double v12 = v[1, 2];
    EXPECT_DOUBLE_EQ(v00, 1.0);
    EXPECT_DOUBLE_EQ(v12, 6.0);
}

TEST(AgentMetricsGridTest, AgentSlice) {
    AgentMetricsGrid grid(2, 3);
    grid.set(1, 0, 10.0);
    grid.set(1, 1, 20.0);
    grid.set(1, 2, 30.0);

    auto slice = grid.agent_slice(1);
    ASSERT_EQ(slice.size(), 3u);
    EXPECT_DOUBLE_EQ(slice[0], 10.0);
    EXPECT_DOUBLE_EQ(slice[1], 20.0);
    EXPECT_DOUBLE_EQ(slice[2], 30.0);
}

TEST(AgentMetricsGridTest, RowMeans) {
    AgentMetricsGrid grid(2, 3);
    // Agent 0: 1, 2, 3 → mean = 2.0
    grid.set(0, 0, 1.0);
    grid.set(0, 1, 2.0);
    grid.set(0, 2, 3.0);
    // Agent 1: 4, 5, 6 → mean = 5.0
    grid.set(1, 0, 4.0);
    grid.set(1, 1, 5.0);
    grid.set(1, 2, 6.0);

    auto means = grid.row_means();
    ASSERT_EQ(means.size(), 2u);
    EXPECT_DOUBLE_EQ(means[0], 2.0);
    EXPECT_DOUBLE_EQ(means[1], 5.0);
}

TEST(AgentMetricsGridTest, ColMeans) {
    AgentMetricsGrid grid(2, 3);
    grid.set(0, 0, 1.0);
    grid.set(0, 1, 2.0);
    grid.set(0, 2, 3.0);
    grid.set(1, 0, 3.0);
    grid.set(1, 1, 4.0);
    grid.set(1, 2, 5.0);
    // Col 0: (1+3)/2=2.0, Col 1: (2+4)/2=3.0, Col 2: (3+5)/2=4.0

    auto means = grid.col_means();
    ASSERT_EQ(means.size(), 3u);
    EXPECT_DOUBLE_EQ(means[0], 2.0);
    EXPECT_DOUBLE_EQ(means[1], 3.0);
    EXPECT_DOUBLE_EQ(means[2], 4.0);
}

TEST(AgentMetricsGridTest, Dimensions) {
    AgentMetricsGrid grid(5, 10);
    EXPECT_EQ(grid.num_agents(), 5u);
    EXPECT_EQ(grid.num_timesteps(), 10u);
}

// --- Coverage: line 36 (agent_slice out of bounds) ---
TEST(AgentMetricsGridTest, AgentSliceOutOfBoundsThrows) {
    AgentMetricsGrid grid(2, 3);
    EXPECT_THROW((void)grid.agent_slice(5), std::out_of_range);
}

// --- Coverage: line 50 (row_means with zero timesteps) ---
TEST(AgentMetricsGridTest, RowMeansZeroTimesteps) {
    AgentMetricsGrid grid(2, 0);
    auto means = grid.row_means();
    ASSERT_EQ(means.size(), 2u);
    EXPECT_DOUBLE_EQ(means[0], 0.0);
    EXPECT_DOUBLE_EQ(means[1], 0.0);
}

// --- Coverage: line 62 (col_means with zero agents) ---
TEST(AgentMetricsGridTest, ColMeansZeroAgents) {
    AgentMetricsGrid grid(0, 3);
    auto means = grid.col_means();
    ASSERT_EQ(means.size(), 3u);
    EXPECT_DOUBLE_EQ(means[0], 0.0);
    EXPECT_DOUBLE_EQ(means[1], 0.0);
    EXPECT_DOUBLE_EQ(means[2], 0.0);
}

} // namespace
} // namespace euxis::metrics
