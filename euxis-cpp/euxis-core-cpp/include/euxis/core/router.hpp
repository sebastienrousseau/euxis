#pragma once

#include <map>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace euxis::core {

/**
 * @brief Performance and cost metrics for an AI model provider.
 */
struct ProviderMetrics {
    std::string name;           ///< Unique identifier for the provider (e.g. "groq").
    double cost_per_1k_tokens;  ///< Estimated cost in USD per 1000 tokens.
    int avg_latency_ms;         ///< Historic average response time in milliseconds.
    double reliability_score;   ///< Normalized 0.0-1.0 success rate.
};

/**
 * @brief Recorded usage data for a single agent session.
 */
struct SessionUsageRecord {
    std::string agent_id;       ///< ID of the agent performing the task.
    std::string model;          ///< Specific LLM model string used.
    int input_tokens{0};        ///< Number of prompt tokens.
    int output_tokens{0};       ///< Number of response tokens.
    double cost{0.0};           ///< Calculated cost for this specific call.
};

/**
 * @brief Intelligent router that optimizes for cost, speed, or reliability.
 * 
 * The FinOpsRouter implements "Financial Operations" for AI, ensuring that requests
 * are routed to the most efficient provider based on task complexity and budget limits.
 */
class FinOpsRouter {
public:
    /**
     * @brief Construct a new FinOps Router.
     * @param budget_limit Global soft limit for total session spend.
     */
    explicit FinOpsRouter(double budget_limit = 10.0);

    /**
     * @brief Select the best provider for a given task.
     * @param task_complexity "low", "medium", or "high".
     * @param priority "speed", "cost", or "balanced".
     * @return std::string The name of the selected provider.
     */
    [[nodiscard]] auto select_provider(const std::string& task_complexity,
                                       const std::string& priority = "balanced")
        -> std::string;

    /** @brief Manually track token usage for a provider. */
    void track_usage(const std::string& provider_name, int tokens);

    /** @brief Get the total accumulated spend for the current session. */
    [[nodiscard]] auto current_spend() const -> double { return current_spend_; }

    /**
     * @brief Track detailed usage within a specific session context.
     * @param session_id Unique identifier for the chat/job session.
     * @param agent_id The agent involved.
     * @param model The specific model used.
     * @param input_tokens Input token count.
     * @param output_tokens Output token count.
     */
    void track_session_usage(const std::string& session_id,
                              const std::string& agent_id,
                              const std::string& model,
                              int input_tokens, int output_tokens);

    /** @brief Get the total cost for a specific session. */
    [[nodiscard]] auto session_cost(const std::string& session_id) const -> double;

    /** @brief Check if a session is still within a specific budget limit. */
    [[nodiscard]] auto check_budget(const std::string& session_id,
                                     double limit) const -> bool;

private:
    double budget_limit_;
    double current_spend_{0.0};
    std::vector<ProviderMetrics> providers_;
    std::map<std::string, std::vector<SessionUsageRecord>> session_usage_;
};

} // namespace euxis::core
