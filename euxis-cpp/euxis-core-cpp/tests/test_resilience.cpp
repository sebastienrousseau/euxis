#include <gtest/gtest.h>

#include "euxis/core/resilience.hpp"

namespace euxis::core {
namespace {

TEST(RetryPolicyTest, DefaultValues) {
    RetryPolicy p;
    EXPECT_EQ(p.max_attempts, 3);
    EXPECT_DOUBLE_EQ(p.base_delay_seconds, 0.05);
}

TEST(RetryPolicyTest, SleepDurationBounded) {
    RetryPolicy p{.max_attempts = 5, .max_delay_seconds = 1.0};
    for (int i = 1; i <= 10; ++i) {
        auto d = p.sleep_duration(i);
        EXPECT_GE(d, 0.0);
        EXPECT_LE(d, p.max_delay_seconds + p.max_delay_seconds * p.jitter_ratio);
    }
}

TEST(RetryPolicyTest, ExponentialGrowth) {
    RetryPolicy p{.jitter_ratio = 0.0};
    EXPECT_DOUBLE_EQ(p.sleep_duration(1), 0.05);
    EXPECT_DOUBLE_EQ(p.sleep_duration(2), 0.10);
    EXPECT_DOUBLE_EQ(p.sleep_duration(3), 0.20);
}

TEST(CircuitBreakerTest, StartsClose) {
    CircuitBreaker cb;
    EXPECT_FALSE(cb.is_open());
}

TEST(CircuitBreakerTest, OpensAfterThreshold) {
    CircuitBreaker cb(3, 10.0);
    cb.record_failure();
    cb.record_failure();
    EXPECT_FALSE(cb.is_open());
    cb.record_failure();
    EXPECT_TRUE(cb.is_open());
}

TEST(CircuitBreakerTest, SuccessResets) {
    CircuitBreaker cb(2, 10.0);
    cb.record_failure();
    cb.record_success();
    cb.record_failure();
    EXPECT_FALSE(cb.is_open());
}

TEST(CircuitBreakerTest, ResetClearsState) {
    CircuitBreaker cb(2, 10.0);
    cb.record_failure();
    cb.record_failure();
    EXPECT_TRUE(cb.is_open());
    cb.reset();
    EXPECT_FALSE(cb.is_open());
}

} // namespace
} // namespace euxis::core
