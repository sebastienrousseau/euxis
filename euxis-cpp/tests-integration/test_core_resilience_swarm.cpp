#include <gtest/gtest.h>

#include "euxis/core/resilience.hpp"

namespace euxis::integration {
namespace {

TEST(CoreResilienceIntegration, RetryPolicyExponentialBackoff) {
    core::RetryPolicy policy{
        .max_attempts = 5,
        .base_delay_seconds = 0.01,
        .max_delay_seconds = 0.5,
        .jitter_ratio = 0.0  // Disable jitter for deterministic test
    };

    double prev = 0.0;
    for (int i = 1; i <= 5; ++i) {
        auto delay = policy.sleep_duration(i);
        EXPECT_GE(delay, prev) << "Delay should increase with attempt number";
        EXPECT_LE(delay, policy.max_delay_seconds);
        prev = delay;
    }
}

TEST(CoreResilienceIntegration, CircuitBreakerOpensAndRecovers) {
    core::CircuitBreaker breaker(3, 0.01);

    // Record failures up to threshold
    breaker.record_failure();
    breaker.record_failure();
    EXPECT_FALSE(breaker.is_open());

    breaker.record_failure();
    EXPECT_TRUE(breaker.is_open());

    // Manual reset simulates recovery
    breaker.reset();
    EXPECT_FALSE(breaker.is_open());
}

TEST(CoreResilienceIntegration, CircuitBreakerSuccessResetsCounts) {
    core::CircuitBreaker breaker(3, 10.0);

    breaker.record_failure();
    breaker.record_failure();
    breaker.record_success();  // Should reset failure count

    // Need 3 more failures to trip
    breaker.record_failure();
    breaker.record_failure();
    EXPECT_FALSE(breaker.is_open());

    breaker.record_failure();
    EXPECT_TRUE(breaker.is_open());
}

TEST(CoreResilienceIntegration, RetryPolicyWithJitter) {
    core::RetryPolicy policy{
        .max_attempts = 3,
        .base_delay_seconds = 0.1,
        .jitter_ratio = 0.2
    };

    // With jitter, delays should still be bounded
    for (int i = 1; i <= 10; ++i) {
        auto delay = policy.sleep_duration(i);
        EXPECT_GE(delay, 0.0);
        EXPECT_LE(delay, policy.max_delay_seconds +
                         policy.max_delay_seconds * policy.jitter_ratio);
    }
}

TEST(CoreResilienceIntegration, MultipleBreakersIndependent) {
    core::CircuitBreaker breaker_a(2, 10.0);
    core::CircuitBreaker breaker_b(3, 10.0);

    breaker_a.record_failure();
    breaker_a.record_failure();
    EXPECT_TRUE(breaker_a.is_open());
    EXPECT_FALSE(breaker_b.is_open());

    breaker_b.record_failure();
    breaker_b.record_failure();
    breaker_b.record_failure();
    EXPECT_TRUE(breaker_b.is_open());
}

} // namespace
} // namespace euxis::integration
