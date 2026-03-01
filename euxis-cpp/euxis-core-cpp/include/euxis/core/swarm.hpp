#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "resilience.hpp"
#include "router.hpp"
#include "types.hpp"

namespace euxis::core {

class SwarmOrchestrator {
public:
    explicit SwarmOrchestrator(const std::string& gateway_url,
                               const std::string& token = {});

    /// Execute a playbook (supports sequential and parallel phase modes).
    auto execute_playbook(const nlohmann::json& playbook,
                          const std::string& goal)
        -> std::vector<nlohmann::json>;

private:
    std::string gateway_url_;
    std::string token_;
    FinOpsRouter router_;
    RetryPolicy connection_retry_;
    CircuitBreaker connection_breaker_;
    bool simulation_mode_;
    std::vector<nlohmann::json> history_;
    std::mutex history_mutex_;

    void execute_phase(const nlohmann::json& phase, const std::string& goal);
    void execute_phase_parallel(std::vector<SwarmTask>& tasks);
    void execute_phase_sequential(std::vector<SwarmTask>& tasks);
    void run_task(SwarmTask& task);
    void record_task(const SwarmTask& task, const std::string& provider);
};

} // namespace euxis::core
