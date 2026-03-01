#include "euxis/core/router.hpp"

#include <algorithm>

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

    if (task_complexity == "low") return "ollama";

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
        double cost_factor = p.cost_per_1k_tokens * 100.0;
        double latency_factor = static_cast<double>(p.avg_latency_ms) / 1000.0;
        double score = (p.reliability_score * 10.0) -
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
            double cost =
                (static_cast<double>(tokens) / 1000.0) * p.cost_per_1k_tokens;
            current_spend_ += cost;
            spdlog::info("Current session spend: ${:.4f}", current_spend_);
            return;
        }
    }
}

} // namespace euxis::core
