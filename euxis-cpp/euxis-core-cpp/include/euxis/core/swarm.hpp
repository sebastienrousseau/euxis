#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "resilience.hpp"
#include "router.hpp"
#include "types.hpp"

namespace euxis::core {

/**
 * @brief Orchestrates multi-agent task execution across a swarm.
 * 
 * The SwarmOrchestrator manages the lifecycle of playbooks, delegating tasks to specific agents
 * via a gateway or local simulation. It handles both sequential and parallel execution phases
 * and tracks execution history.
 */
class SwarmOrchestrator {
public:
    /**
     * @brief Construct a new Swarm Orchestrator.
     * @param gateway_url The HTTP/WS URL of the agent gateway.
     * @param token Optional authentication token for the gateway.
     */
    explicit SwarmOrchestrator(const std::string& gateway_url,
                               const std::string& token = {});

    /**
     * @brief Execute a playbook defined in JSON format.
     * @param playbook The playbook manifest containing phases and delegate tasks.
     * @param goal The global objective for this playbook run.
     * @return std::vector<nlohmann::json> The complete execution history/results.
     */
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

    /** @brief Execute a single phase of a playbook. */
    void execute_phase(const nlohmann::json& phase, const std::string& goal);
    
    /** @brief Execute a set of tasks in parallel. */
    void execute_phase_parallel(std::vector<SwarmTask>& tasks);
    
    /** @brief Execute a set of tasks one by one. */
    void execute_phase_sequential(std::vector<SwarmTask>& tasks);
    
    /** @brief Run a single task via the gateway or simulation. */
    void run_task(SwarmTask& task);
    
    /** @brief Record a task result in the history. */
    void record_task(const SwarmTask& task, const std::string& provider);
};

} // namespace euxis::core
