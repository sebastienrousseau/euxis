#include "euxis/core/supervisor.hpp"
#include <spdlog/spdlog.h>

namespace euxis::core {

SupervisorAgent::SupervisorAgent(std::shared_ptr<FinOpsRouter> router, 
                                 std::shared_ptr<network::CircuitBreaker> cb)
    : router_(std::move(router)), cb_(std::move(cb)) {}

SupervisorAgent::~SupervisorAgent() {
    stop();
}

void SupervisorAgent::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread(&SupervisorAgent::monitor_loop, this);
}

void SupervisorAgent::stop() {
    if (running_.exchange(false)) {
        if (worker_.joinable()) worker_.join();
    }
}

void SupervisorAgent::monitor_loop() {
    spdlog::info("SupervisorAgent started. Monitoring system health.");
    
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Adaptive Autonomy: Check Circuit Breaker State
        if (cb_->is_open()) {
            spdlog::warn("Supervisor detected OPEN circuit breaker. Initiating degraded mode.");
            // Reduce session memory limits aggressively during high failure periods
            router_->set_session_limit(10); 
        } else {
            // Restore normal operating bounds
            if (router_->get_session_limit() < 100) {
                spdlog::info("Supervisor restoring normal session limits.");
                router_->set_session_limit(100);
            }
        }
    }
}

} // namespace euxis::core
