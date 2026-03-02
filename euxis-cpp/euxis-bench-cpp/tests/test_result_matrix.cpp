#include <gtest/gtest.h>
#include <cmath>

#include "euxis/bench/result_matrix.hpp"

namespace euxis::bench {
namespace {

TEST(BenchmarkResultMatrixTest, RecordAndView) {
    BenchmarkResultMatrix mat(3, {"latency", "throughput"});
    mat.record(0, 0, 10.0);
    mat.record(0, 1, 100.0);
    mat.record(1, 0, 20.0);
    mat.record(1, 1, 200.0);
    mat.record(2, 0, 30.0);
    mat.record(2, 1, 300.0);

    auto v = mat.view();
    double v00 = v[0, 0];
    double v21 = v[2, 1];
    EXPECT_DOUBLE_EQ(v00, 10.0);
    EXPECT_DOUBLE_EQ(v21, 300.0);
}

TEST(BenchmarkResultMatrixTest, ColumnMean) {
    BenchmarkResultMatrix mat(3, {"val"});
    mat.record(0, 0, 10.0);
    mat.record(1, 0, 20.0);
    mat.record(2, 0, 30.0);

    EXPECT_DOUBLE_EQ(mat.column_mean(0), 20.0);
}

TEST(BenchmarkResultMatrixTest, ColumnStddev) {
    BenchmarkResultMatrix mat(3, {"val"});
    mat.record(0, 0, 10.0);
    mat.record(1, 0, 20.0);
    mat.record(2, 0, 30.0);
    // mean=20, variance = ((100+0+100)/2) = 100, stddev = 10
    EXPECT_NEAR(mat.column_stddev(0), 10.0, 1e-9);
}

TEST(BenchmarkResultMatrixTest, ColumnPercentile) {
    BenchmarkResultMatrix mat(5, {"val"});
    mat.record(0, 0, 1.0);
    mat.record(1, 0, 2.0);
    mat.record(2, 0, 3.0);
    mat.record(3, 0, 4.0);
    mat.record(4, 0, 5.0);

    EXPECT_DOUBLE_EQ(mat.column_percentile(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(mat.column_percentile(0, 100), 5.0);
    EXPECT_NEAR(mat.column_percentile(0, 50), 3.0, 1e-9);
}

TEST(BenchmarkResultMatrixTest, OutOfBoundsThrows) {
    BenchmarkResultMatrix mat(2, {"a"});
    EXPECT_THROW(mat.record(5, 0, 1.0), std::out_of_range);
    EXPECT_THROW((void)mat.column_mean(5), std::out_of_range);
}

TEST(BenchmarkResultMatrixTest, MetricNames) {
    BenchmarkResultMatrix mat(1, {"latency_ms", "throughput_rps", "error_rate"});
    auto names = mat.metric_names();
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "latency_ms");
    EXPECT_EQ(names[2], "error_rate");
}

TEST(BenchmarkResultMatrixTest, Dimensions) {
    BenchmarkResultMatrix mat(10, {"a", "b"});
    EXPECT_EQ(mat.iterations(), 10u);
    EXPECT_EQ(mat.num_metrics(), 2u);
}

TEST(BenchmarkResultMatrixTest, ColumnMeanZeroIterations) {
    // Zero iterations edge case (covers line 45: iterations_ == 0 returns 0.0)
    BenchmarkResultMatrix mat(0, {"val"});
    EXPECT_DOUBLE_EQ(mat.column_mean(0), 0.0);
}

TEST(BenchmarkResultMatrixTest, ColumnStddevSingleIteration) {
    // Single iteration edge case (covers line 60: iterations_ <= 1 returns 0.0)
    BenchmarkResultMatrix mat(1, {"val"});
    mat.record(0, 0, 42.0);
    EXPECT_DOUBLE_EQ(mat.column_stddev(0), 0.0);
}

TEST(BenchmarkResultMatrixTest, ColumnPercentileZeroIterations) {
    // Zero iterations for percentile
    BenchmarkResultMatrix mat(0, {"val"});
    EXPECT_DOUBLE_EQ(mat.column_percentile(0, 50), 0.0);
}

TEST(BenchmarkResultMatrixTest, ColumnStddevOutOfBoundsThrows) {
    BenchmarkResultMatrix mat(2, {"a"});
    EXPECT_THROW((void)mat.column_stddev(5), std::out_of_range);
}

TEST(BenchmarkResultMatrixTest, ColumnPercentileOutOfBoundsThrows) {
    BenchmarkResultMatrix mat(2, {"a"});
    EXPECT_THROW((void)mat.column_percentile(5, 50), std::out_of_range);
}

} // namespace
} // namespace euxis::bench
