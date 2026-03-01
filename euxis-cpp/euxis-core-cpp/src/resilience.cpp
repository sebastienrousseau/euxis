#include "euxis/core/resilience.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <thread>

namespace euxis::core {
namespace {

auto now_epoch() -> double {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

thread_local std::mt19937 rng{std::random_device{}()};

} // namespace

auto RetryPolicy::sleep_duration(int attempt) const -> double {
    double raw =
        std::min(base_delay_seconds * std::pow(2.0, std::max(0, attempt - 1)),
                 max_delay_seconds);
    double jitter = raw * jitter_ratio;
    std::uniform_real_distribution<double> dist(-jitter, jitter);
    return std::max(0.0, raw + dist(rng));
}

CircuitBreaker::CircuitBreaker(int failure_threshold,
                               double recovery_timeout_seconds)
    : failure_threshold_(failure_threshold),
      recovery_timeout_seconds_(recovery_timeout_seconds) {}

auto CircuitBreaker::is_open() -> bool {
    if (!opened_at_) return false;
    if ((now_epoch() - *opened_at_) >= recovery_timeout_seconds_) {
        reset();
        return false;
    }
    return true;
}

void CircuitBreaker::record_success() { reset(); }

void CircuitBreaker::record_failure() {
    ++failure_count_;
    if (failure_count_ >= failure_threshold_) {
        opened_at_ = now_epoch();
    }
}

void CircuitBreaker::reset() {
    failure_count_ = 0;
    opened_at_.reset();
}

} // namespace euxis::core
