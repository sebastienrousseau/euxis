#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <chrono>

#include "euxis/core/router.hpp"
#include "euxis/network/resilience.hpp"

namespace euxis::core {

/**
 * @brief Autonomous Supervisor Daemon that monitors and tunes core components.
 * 
 * Implements self-healing by adjusting LRU limits and resetting breakers based on telemetry.
 */
class SupervisorAgent {
public:
    SupervisorAgent(std::shared_ptr<FinOpsRouter> router, 
                    std::shared_ptr<network::CircuitBreaker> cb);
    ~SupervisorAgent();

    void start();
    void stop();

private:
    void monitor_loop();

    std::shared_ptr<FinOpsRouter> router_;
    std::shared_ptr<network::CircuitBreaker> cb_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace euxis::core
