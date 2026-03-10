#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

/** @namespace euxis::core
 * @brief Core abstractions for agent orchestration, resilience, and networking.
 */
namespace euxis::core {

/**
 * @brief Configurable policy for exponential backoff with jitter.
 */
struct RetryPolicy {
    int max_attempts{3};            ///< Maximum number of execution attempts.
    double base_delay_seconds{0.05}; ///< Initial delay before first retry.
    double max_delay_seconds{1.0};   ///< Maximum cap for delay between retries.
    double jitter_ratio{0.1};        ///< Randomness factor to prevent thundering herd.

    /**
     * @brief Calculate the sleep duration for a specific attempt.
     * @param attempt The 1-based attempt count.
     * @return double Sleep duration in seconds.
     */
    [[nodiscard]] auto sleep_duration(int attempt) const -> double;
};

/**
 * @brief State-based failure protection to prevent cascading system failures.
 */
class CircuitBreaker {
public:
    /**
     * @brief Construct a new Circuit Breaker.
     * @param failure_threshold Number of consecutive failures before opening.
     * @param recovery_timeout_seconds Seconds to wait in Open state before attempting reset.
     */
    explicit CircuitBreaker(int failure_threshold = 5,
                            double recovery_timeout_seconds = 10.0);

    /** @brief Returns true if the circuit is currently open (blocking). */
    [[nodiscard]] auto is_open() -> bool;
    
    /** @brief Signals a successful operation, resetting failure counters. */
    void record_success();
    
    /** @brief Signals an operational failure, potentially opening the circuit. */
    void record_failure();
    
    /** @brief Force-resets the breaker to Closed state. */
    void reset();

private:
    int failure_threshold_;
    double recovery_timeout_seconds_;
    int failure_count_{0};
    std::optional<double> opened_at_;
};

/**
 * @brief Helper to get a shared default circuit breaker.
 * @return CircuitBreaker& Static shared instance.
 */
inline auto get_default_breaker() -> CircuitBreaker& {
    static CircuitBreaker breaker;
    return breaker;
}

/**
 * @brief Higher-order function to execute an operation with resilience patterns.
 * 
 * @tparam F Type of the callable operation.
 * @param operation The code to execute.
 * @param retry The retry policy to apply.
 * @param breaker Reference to a circuit breaker to use.
 * @return decltype(operation()) The result of the operation.
 * @throws std::runtime_error if retries are exhausted or circuit is open.
 */
template <typename F>
auto run_with_resilience(F&& operation,
                         const RetryPolicy& retry = {},
                         CircuitBreaker& breaker = get_default_breaker())
    -> decltype(operation());

} // namespace euxis::core
