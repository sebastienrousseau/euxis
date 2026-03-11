#include <euxis/network/resilience.hpp>
#include <random>
#include <thread>
#include <cmath>

namespace euxis::network {

auto RetryPolicy::sleep_duration(int attempt) const -> double {
    if (attempt <= 1) return 0.0;
    
    // Exponential backoff: base * 2^(attempt-1)
    double delay = base_delay_seconds * std::pow(2.0, attempt - 1);
    
    // Cap the delay
    if (delay > max_delay_seconds) delay = max_delay_seconds;
    
    // Apply jitter
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(1.0 - jitter_ratio, 1.0 + jitter_ratio);
    
    return delay * dis(gen);
}

CircuitBreaker::CircuitBreaker(int failure_threshold, double recovery_timeout_seconds)
    : failure_threshold_(failure_threshold), recovery_timeout_seconds_(recovery_timeout_seconds) {}

auto CircuitBreaker::is_open() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_at_) return false;
    
    // Check if recovery timeout has passed
    auto now = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count());
    
    if (now - *opened_at_ >= recovery_timeout_seconds_) {
        // Half-open state: allow a trial
        return false;
    }
    
    return true;
}

void CircuitBreaker::record_success() {
    std::lock_guard<std::mutex> lock(mutex_);
    failure_count_ = 0;
    opened_at_ = std::nullopt;
}

void CircuitBreaker::record_failure() {
    std::lock_guard<std::mutex> lock(mutex_);
    failure_count_++;
    if (failure_count_ >= failure_threshold_) {
        opened_at_ = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count());
    }
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    failure_count_ = 0;
    opened_at_ = std::nullopt;
}

} // namespace euxis::network
