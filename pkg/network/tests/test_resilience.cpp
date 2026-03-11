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

TEST(RetryPolicyTest, SleepDurationBounded) {
    RetryPolicy p{.max_attempts = 5, .max_delay_seconds = 1.0};
    for (int i = 1; i <= 5; ++i) {
        auto d = p.sleep_duration(i);
        EXPECT_GE(d, 0.0);
    }
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

TEST(CircuitBreakerTest, ConcurrentFailureRecording) {
    CircuitBreaker cb(1000, 10.0);
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&cb]() {
            for (int j = 0; j < 100; ++j) {
                cb.record_failure();
            }
        });
    }
    for (auto& t : threads) t.join();
    
    // If it was not thread-safe, failure_count_ might be less than 1000
    // and thus not open. With 1000 records, it should be exactly on the threshold.
    EXPECT_TRUE(cb.is_open());
}

TEST(CircuitBreakerTest, RecoverAfterTimeout) {
    CircuitBreaker cb(1, 0.1); 
    cb.record_failure();
    EXPECT_TRUE(cb.is_open());
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_FALSE(cb.is_open());
}

} // namespace
} // namespace euxis::network
