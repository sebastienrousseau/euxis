#pragma once

#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace euxis::core {

struct ProviderMetrics {
    std::string name;
    double cost_per_1k_tokens;
    int avg_latency_ms;
    double reliability_score;
};

class FinOpsRouter {
public:
    explicit FinOpsRouter(double budget_limit = 10.0);

    [[nodiscard]] auto select_provider(const std::string& task_complexity,
                                       const std::string& priority = "balanced")
        -> std::string;

    void track_usage(const std::string& provider_name, int tokens);

    [[nodiscard]] auto current_spend() const -> double { return current_spend_; }

private:
    double budget_limit_;
    double current_spend_{0.0};
    std::vector<ProviderMetrics> providers_;
};

} // namespace euxis::core
