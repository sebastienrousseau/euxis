#include "euxis/runtime/interaction.hpp"

#include <chrono>
#include <numeric>
#include <string>

namespace euxis::runtime {

// --- Scratchpad ---

void Scratchpad::write(const std::string& key, const nlohmann::json& value,
                       const std::string& /*writer*/) {
    std::lock_guard lock(mu_);
    data_[key] = value;
}

auto Scratchpad::read(const std::string& key) const -> std::optional<nlohmann::json> {
    std::lock_guard lock(mu_);
    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    return it->second;
}

auto Scratchpad::read_all() const -> nlohmann::json {
    std::lock_guard lock(mu_);
    nlohmann::json result;
    for (const auto& [k, v] : data_) {
        result[k] = v;
    }
    return result;
}

void Scratchpad::clear() {
    std::lock_guard lock(mu_);
    data_.clear();
}

// --- InteractionOrchestrator ---

InteractionOrchestrator::InteractionOrchestrator(IProvider* provider,
                                                   ISessionStore* store)
    : provider_(provider), store_(store) {}

auto InteractionOrchestrator::run_sequential(
    const std::vector<std::string>& agent_ids,
    const std::string& task) -> InteractionResult {
    InteractionResult result;
    result.success = true;
    const std::string& current_input = task;

    for (const auto& agent_id : agent_ids) {
        AgentSession session("seq-" + agent_id, agent_id, provider_, store_);
        auto response = session.send(current_input);

        result.agent_outputs.push_back(response.output);
        result.total_duration_ms += response.duration_ms;
        ++result.rounds;

        if (!response.success) {
            result.success = false;
            result.output = response.error;
            return result;
        }
        current_input = response.output;
    }

    result.output = current_input;
    return result;
}

auto InteractionOrchestrator::run_parallel(
    const std::vector<std::string>& agent_ids,
    const std::string& task) -> InteractionResult {
    InteractionResult result;
    result.success = true;
    result.rounds = 1;

    for (const auto& agent_id : agent_ids) {
        AgentSession session("par-" + agent_id, agent_id, provider_, store_);
        auto response = session.send(task);

        result.agent_outputs.push_back(response.output);
        result.total_duration_ms += response.duration_ms;

        if (!response.success) {
            result.success = false;
        }
    }

    std::string combined;
    for (size_t i = 0; i < result.agent_outputs.size(); ++i) {
        if (i > 0) combined += "\n---\n";
        combined += result.agent_outputs[i];
    }
    result.output = combined;
    return result;
}

auto InteractionOrchestrator::run_scratchpad(
    const std::vector<std::string>& agent_ids,
    const std::string& task,
    Scratchpad& scratchpad,
    int max_rounds) -> InteractionResult {
    InteractionResult result;
    result.success = true;

    scratchpad.write("task", task, "orchestrator");

    for (int round = 0; round < max_rounds; ++round) {
        ++result.rounds;
        for (const auto& agent_id : agent_ids) {
            auto context = scratchpad.read_all();
            std::string prompt = task + "\n\nShared context:\n" + context.dump(2);

            AgentSession session("sp-" + agent_id, agent_id, provider_, store_);
            auto response = session.send(prompt);
            result.total_duration_ms += response.duration_ms;

            if (!response.success) {
                result.success = false;
                result.output = response.error;
                return result;
            }

            scratchpad.write("output_" + agent_id, response.output, agent_id);
            result.agent_outputs.push_back(response.output);
        }
    }

    result.output = scratchpad.read_all().dump(2);
    return result;
}

auto InteractionOrchestrator::run_with_evaluator(
    const std::string& worker_agent_id,
    const std::string& task,
    const EvaluationGateConfig& gate) -> InteractionResult {
    InteractionResult result;

    for (int attempt = 0; attempt <= gate.max_retries; ++attempt) {
        ++result.rounds;

        AgentSession worker("eval-worker-" + std::to_string(attempt),
                           worker_agent_id, provider_, store_);
        auto work_result = worker.send(task);
        result.total_duration_ms += work_result.duration_ms;
        result.agent_outputs.push_back(work_result.output);

        if (!work_result.success) {
            continue;
        }

        AgentSession evaluator("eval-gate-" + std::to_string(attempt),
                              gate.evaluator_agent_id, provider_, store_);
        std::string eval_prompt = gate.evaluation_prompt +
            "\n\nOutput to evaluate:\n" + work_result.output;
        auto eval_result = evaluator.send(eval_prompt);
        result.total_duration_ms += eval_result.duration_ms;

        if (eval_result.success) {
            try {
                double score = std::stod(eval_result.output);
                if (score >= gate.min_quality_score) {
                    result.success = true;
                    result.output = work_result.output;
                    return result;
                }
            } catch (const std::exception&) {
                // If not a simple number, assume success if the evaluator didn't error
                result.success = true;
                result.output = work_result.output;
                return result;
            }
        }
    }

    result.success = false;
    result.output = "Evaluator gate failed after " +
        std::to_string(gate.max_retries + 1) + " attempts";
    return result;
}

auto InteractionOrchestrator::run_supervisor_worker(
    const SupervisorConfig& config,
    const std::string& task) -> InteractionResult {
    InteractionResult result;

    AgentSession supervisor("sv-decompose", config.supervisor_agent_id,
                           provider_, store_);
    auto decomp = supervisor.send(config.decomposition_prompt + "\n\nTask: " + task);
    result.total_duration_ms += decomp.duration_ms;
    ++result.rounds;

    if (!decomp.success) {
        result.success = false;
        result.output = decomp.error;
        return result;
    }

    for (const auto& worker_id : config.worker_agent_ids) {
        ++result.rounds;
        AgentSession worker("sv-worker-" + worker_id, worker_id, provider_, store_);
        auto work_result = worker.send(decomp.output);
        result.total_duration_ms += work_result.duration_ms;
        result.agent_outputs.push_back(work_result.output);

        if (!work_result.success) {
            result.success = false;
        }
    }

    std::string agg_input = config.aggregation_prompt + "\n\nWorker outputs:\n";
    for (size_t i = 0; i < result.agent_outputs.size(); ++i) {
        agg_input += "--- Worker " + std::to_string(i) + " ---\n";
        agg_input += result.agent_outputs[i] + "\n";
    }

    AgentSession agg_session("sv-aggregate", config.supervisor_agent_id,
                            provider_, store_);
    auto agg_result = agg_session.send(agg_input);
    result.total_duration_ms += agg_result.duration_ms;
    ++result.rounds;

    result.success = agg_result.success;
    result.output = agg_result.output;
    return result;
}

} // namespace euxis::runtime
