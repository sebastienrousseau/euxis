#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include "euxis/network/resilience.hpp"

namespace euxis::network {
namespace {

TEST(RetryPolicyTest, DefaultValues) {
    RetryPolicy p;
    EXPECT_EQ(p.max_attempts, 3);
    EXPECT_DOUBLE_EQ(p.base_delay_seconds, 0.05);
}

TEST(RetryPolicyTest, SleepDurationCapping) {
    RetryPolicy p{.max_delay_seconds = 0.5, .jitter_ratio = 0.0};
    // 0.05 * 2^10 is way over 0.5
    EXPECT_DOUBLE_EQ(p.sleep_duration(10), 0.5);
    EXPECT_DOUBLE_EQ(p.sleep_duration(1), 0.0);
}

TEST(CircuitBreakerTest, BasicFlow) {
    CircuitBreaker cb(2, 0.1);
    EXPECT_FALSE(cb.is_open());
    
    cb.record_failure();
    EXPECT_FALSE(cb.is_open());
    
    cb.record_failure();
    EXPECT_TRUE(cb.is_open());
    
    cb.record_success();
    EXPECT_FALSE(cb.is_open());
}

TEST(CircuitBreakerTest, Reset) {
    CircuitBreaker cb(1);
    cb.record_failure();
    EXPECT_TRUE(cb.is_open());
    cb.reset();
    EXPECT_FALSE(cb.is_open());
}

TEST(CircuitBreakerTest, RecoverAfterTimeout) {
    CircuitBreaker cb(1, 0.05); 
    cb.record_failure();
    EXPECT_TRUE(cb.is_open());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(cb.is_open());
}

TEST(CircuitBreakerTest, ConcurrentFailureRecording) {
    CircuitBreaker cb(1000, 10.0);
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&cb]() {
            for (int j = 0; j < 100; ++j) cb.record_failure();
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_TRUE(cb.is_open());
}

TEST(ResilienceTest, DefaultBreaker) {
    auto& b1 = get_default_breaker();
    auto& b2 = get_default_breaker();
    EXPECT_EQ(&b1, &b2);
}

// Internal implementation of run_with_resilience for testing coverage
// (since it's a template in the header, we test it via a concrete instantiation)
TEST(ResilienceTest, RunWithResilienceSuccess) {
    int calls = 0;
    auto res = run_with_resilience([&]() {
        calls++;
        return 42;
    });
    EXPECT_EQ(res, 42);
    EXPECT_EQ(calls, 1);
}

} // namespace
} // namespace euxis::network
