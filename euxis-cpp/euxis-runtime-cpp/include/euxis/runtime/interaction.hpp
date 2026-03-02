#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agent_session.hpp"

namespace euxis::runtime {

enum class InteractionPattern {
    Sequential,
    Parallel,
    SharedScratchpad,
    EvaluatorLoop,
    SupervisorWorker,
};

/// Thread-safe shared scratchpad for multi-agent communication.
class Scratchpad {
public:
    void write(const std::string& key, const nlohmann::json& value,
               const std::string& writer);
    [[nodiscard]] auto read(const std::string& key) const -> std::optional<nlohmann::json>;
    [[nodiscard]] auto read_all() const -> nlohmann::json;
    void clear();

private:
    mutable std::mutex mu_;
    std::map<std::string, nlohmann::json> data_;
};

struct InteractionResult {
    bool success{false};
    std::string output;
    std::vector<std::string> agent_outputs;
    int rounds{0};
    double total_duration_ms{0.0};
};

struct EvaluationGateConfig {
    std::string evaluator_agent_id;
    std::string evaluation_prompt;
    int max_retries{3};
    double min_quality_score{0.7};
};

struct SupervisorConfig {
    std::string supervisor_agent_id;
    std::vector<std::string> worker_agent_ids;
    std::string decomposition_prompt;
    std::string aggregation_prompt;
    bool parallel_workers{true};
};

/// Orchestrates multi-agent interaction patterns.
class InteractionOrchestrator {
public:
    explicit InteractionOrchestrator(IProvider* provider, ISessionStore* store);

    [[nodiscard]] auto run_sequential(
        const std::vector<std::string>& agent_ids,
        const std::string& task) -> InteractionResult;

    [[nodiscard]] auto run_parallel(
        const std::vector<std::string>& agent_ids,
        const std::string& task) -> InteractionResult;

    [[nodiscard]] auto run_scratchpad(
        const std::vector<std::string>& agent_ids,
        const std::string& task,
        Scratchpad& scratchpad,
        int max_rounds = 3) -> InteractionResult;

    [[nodiscard]] auto run_with_evaluator(
        const std::string& worker_agent_id,
        const std::string& task,
        const EvaluationGateConfig& gate) -> InteractionResult;

    [[nodiscard]] auto run_supervisor_worker(
        const SupervisorConfig& config,
        const std::string& task) -> InteractionResult;

private:
    IProvider* provider_;
    ISessionStore* store_;
};

} // namespace euxis::runtime
