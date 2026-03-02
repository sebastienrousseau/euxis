#pragma once

#include <map>
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

struct SessionUsageRecord {
    std::string agent_id;
    std::string model;
    int input_tokens{0};
    int output_tokens{0};
    double cost{0.0};
};

class FinOpsRouter {
public:
    explicit FinOpsRouter(double budget_limit = 10.0);

    [[nodiscard]] auto select_provider(const std::string& task_complexity,
                                       const std::string& priority = "balanced")
        -> std::string;

    void track_usage(const std::string& provider_name, int tokens);

    [[nodiscard]] auto current_spend() const -> double { return current_spend_; }

    // --- Session cost tracking ---
    void track_session_usage(const std::string& session_id,
                              const std::string& agent_id,
                              const std::string& model,
                              int input_tokens, int output_tokens);

    [[nodiscard]] auto session_cost(const std::string& session_id) const -> double;

    [[nodiscard]] auto check_budget(const std::string& session_id,
                                     double limit) const -> bool;

private:
    double budget_limit_;
    double current_spend_{0.0};
    std::vector<ProviderMetrics> providers_;
    std::map<std::string, std::vector<SessionUsageRecord>> session_usage_;
};

} // namespace euxis::core
