#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <mutex>

/** @namespace euxis::network
 * @brief Network resilience patterns including retries and circuit breakers.
 */
namespace euxis::network {

/**
 * @brief Configurable policy for exponential backoff with jitter.
 */
struct RetryPolicy {
    int max_attempts{3};            ///< Maximum number of execution attempts.
    double base_delay_seconds{0.05}; ///< Initial delay before first retry.
    double max_delay_seconds{1.0};   ///< Maximum cap for delay between retries.
    double jitter_ratio{0.1};        ///< Randomness factor to prevent thundering herd.

    [[nodiscard]] auto sleep_duration(int attempt) const -> double;
};

/**
 * @brief State-based failure protection to prevent cascading system failures.
 */
class CircuitBreaker {
public:
    explicit CircuitBreaker(int failure_threshold = 5,
                            double recovery_timeout_seconds = 10.0);

    [[nodiscard]] auto is_open() -> bool;
    void record_success();
    void record_failure();
    void reset();

private:
    std::mutex mutex_;
    int failure_threshold_;
    double recovery_timeout_seconds_;
    int failure_count_{0};
    std::optional<double> opened_at_;
};

inline auto get_default_breaker() -> CircuitBreaker& {
    static CircuitBreaker breaker;
    return breaker;
}

/**
 * @brief Higher-order function to execute an operation with resilience patterns.
 */
template <typename F>
auto run_with_resilience(F&& operation,
                         const RetryPolicy& retry = {},
                         CircuitBreaker& breaker = get_default_breaker())
    -> decltype(operation()) {
    
    if (breaker.is_open()) {
        throw std::runtime_error("Circuit breaker is OPEN");
    }

    std::exception_ptr last_exception;
    for (int attempt = 1; attempt <= retry.max_attempts; ++attempt) {
        try {
            auto result = operation();
            breaker.record_success();
            return result;
        } catch (...) {
            last_exception = std::current_exception();
            breaker.record_failure();
            
            if (attempt < retry.max_attempts) {
                double delay = retry.sleep_duration(attempt);
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<long long>(delay * 1000)));
            }
        }
    }

    if (last_exception) {
        std::rethrow_exception(last_exception);
    }
    throw std::runtime_error("Retries exhausted without exception");
}

} // namespace euxis::network
