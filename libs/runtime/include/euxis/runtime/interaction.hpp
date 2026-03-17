/// @file
/// @brief orchestration of multi-agent interactions and workflows.
#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agent_session.hpp"
#include "provider.hpp"
#include "session_store.hpp"

namespace euxis::runtime {

/// @brief Result of a multi-agent interaction workflow.
struct InteractionResult {
    bool success{false};
    std::string output;
    std::vector<std::string> agent_outputs;
    double total_duration_ms{0.0};
    int rounds{0};
};

/// @brief thread-safe shared memory space for agents in an interaction.
class Scratchpad {
public:
    /// @brief write a value to the scratchpad.
    void write(const std::string& key, const nlohmann::json& value,
               const std::string& writer = "");
    
    /// @brief Read a value from the scratchpad.
    auto read(const std::string& key) const -> std::optional<nlohmann::json>;
    
    /// @brief Get all values in the scratchpad as a JSON object.
    auto read_all() const -> nlohmann::json;
    
    /// @brief reset the scratchpad.
    void clear();

private:
    mutable std::mutex mu_;
    std::map<std::string, nlohmann::json> data_;
};

/// @brief Configuration for a quality evaluation gate.
struct EvaluationGateConfig {
    std::string evaluator_agent_id;
    std::string evaluation_prompt;
    int max_retries{2};
    double min_quality_score{0.7};
};

/// @brief configuration for supervisor-worker orchestration patterns.
struct SupervisorConfig {
    std::string supervisor_agent_id;
    std::vector<std::string> worker_agent_ids;
    std::string decomposition_prompt;
    std::string aggregation_prompt;
    int parallel_workers{3};
};

/// @brief Orchestrates agents through complex patterns (sequential, parallel, etc).
class InteractionOrchestrator {
public:
    /// @brief Construct orchestrator with a provider and session store.
    InteractionOrchestrator(IProvider* provider, ISessionStore* store);

    /// @brief run agents in a linear sequence (pipeline).
    auto run_sequential(const std::vector<std::string>& agent_ids,
                        const std::string& task) -> InteractionResult;

    /// @brief run multiple agents in parallel on the same task.
    auto run_parallel(const std::vector<std::string>& agent_ids,
                      const std::string& task) -> InteractionResult;

    /// @brief run agents using a shared scratchpad for collaboration.
    auto run_scratchpad(const std::vector<std::string>& agent_ids,
                        const std::string& task,
                        Scratchpad& scratchpad,
                        int max_rounds = 3) -> InteractionResult;

    /// @brief run an agent and evaluate its output using another agent.
    auto run_with_evaluator(const std::string& worker_agent_id,
                            const std::string& task,
                            const EvaluationGateConfig& gate) -> InteractionResult;

    /// @brief run a hierarchical supervisor-worker workflow.
    auto run_supervisor_worker(const SupervisorConfig& config,
                               const std::string& task) -> InteractionResult;

private:
    IProvider* provider_;
    ISessionStore* store_;
};

} // namespace euxis::runtime
