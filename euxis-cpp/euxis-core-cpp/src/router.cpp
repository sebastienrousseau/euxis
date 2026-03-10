#include "euxis/core/router.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace euxis::core {

FinOpsRouter::FinOpsRouter(double budget_limit) : budget_limit_(budget_limit) {
    providers_ = {
        {"ollama", 0.000, 150, 0.95},
        {"groq", 0.0001, 50, 0.98},
        {"anthropic", 0.015, 800, 0.99},
        {"openai", 0.010, 600, 0.99},
    };
}

auto FinOpsRouter::select_provider(const std::string& task_complexity,
                                   const std::string& priority) -> std::string {
    spdlog::info("Routing task (complexity: {}, priority: {})",
                 task_complexity, priority);

    if (task_complexity == "low") {
        return "ollama";
    }

    if (priority == "speed") {
        auto it = std::ranges::min_element(
            providers_, {}, &ProviderMetrics::avg_latency_ms);
        return it->name;
    }

    if (priority == "cost") {
        auto it = std::ranges::min_element(
            providers_, {}, &ProviderMetrics::cost_per_1k_tokens);
        return it->name;
    }

    // Weighted score balancing reliability, cost, and latency
    std::string best = "ollama";
    double best_score = -100.0;
    for (const auto& p : providers_) {
        const double cost_factor = p.cost_per_1k_tokens * 100.0;
        const double latency_factor = static_cast<double>(p.avg_latency_ms) / 1000.0;
        const double score = (p.reliability_score * 10.0) -
                       (cost_factor * 5.0) - (latency_factor * 2.0);
        if (score > best_score) {
            best_score = score;
            best = p.name;
        }
    }

    spdlog::info("FinOps Router selected: {} (score: {:.2f})", best, best_score);
    return best;
}

void FinOpsRouter::track_usage(const std::string& provider_name, int tokens) {
    for (const auto& p : providers_) {
        if (p.name == provider_name) {
            const double cost =
                (static_cast<double>(tokens) / 1000.0) * p.cost_per_1k_tokens;
            current_spend_ += cost;
            spdlog::info("Current session spend: ${:.4f}", current_spend_);
            return;
        }
    }
}

void FinOpsRouter::track_session_usage(const std::string& session_id,
                                        const std::string& agent_id,
                                        const std::string& model,
                                        int input_tokens, int output_tokens) {
    const int total = input_tokens + output_tokens;
    double cost = 0.0;
    // Estimate cost based on known providers
    for (const auto& p : providers_) {
        if (model.find(p.name) != std::string::npos) {
            cost = (static_cast<double>(total) / 1000.0) * p.cost_per_1k_tokens;
            break;
        }
    }
    if (cost == 0.0) {
        // Default estimate
        cost = static_cast<double>(total) / 1000.0 * 0.001;
    }

    session_usage_[session_id].push_back({
        .agent_id = agent_id,
        .model = model,
        .input_tokens = input_tokens,
        .output_tokens = output_tokens,
        .cost = cost,
    });
    current_spend_ += cost;
}

auto FinOpsRouter::session_cost(const std::string& session_id) const -> double {
    const auto it = session_usage_.find(session_id);
    if (it == session_usage_.end()) {
        return 0.0;
    }

    double total = 0.0;
    for (const auto& r : it->second) {
        total += r.cost;
    }
    return total;
}

auto FinOpsRouter::check_budget(const std::string& session_id,
                                 double limit) const -> bool {
    return session_cost(session_id) <= limit;
}

} // namespace euxis::core
