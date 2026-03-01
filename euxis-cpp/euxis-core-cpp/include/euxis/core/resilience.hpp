#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace euxis::core {

struct RetryPolicy {
    int max_attempts{3};
    double base_delay_seconds{0.05};
    double max_delay_seconds{1.0};
    double jitter_ratio{0.1};

    [[nodiscard]] auto sleep_duration(int attempt) const -> double;
};

class CircuitBreaker {
public:
    explicit CircuitBreaker(int failure_threshold = 5,
                            double recovery_timeout_seconds = 10.0);

    [[nodiscard]] auto is_open() -> bool;
    void record_success();
    void record_failure();
    void reset();

private:
    int failure_threshold_;
    double recovery_timeout_seconds_;
    int failure_count_{0};
    std::optional<double> opened_at_;
};

/// Run an operation with retry + circuit breaker protection.
/// Throws std::runtime_error on exhaustion.
template <typename F>
auto run_with_resilience(F&& operation,
                         const RetryPolicy& retry = {},
                         CircuitBreaker& breaker = *(new CircuitBreaker()))
    -> decltype(operation());

} // namespace euxis::core
