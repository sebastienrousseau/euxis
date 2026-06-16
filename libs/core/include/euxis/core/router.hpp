#pragma once

#include <atomic>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
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
 * Uses Structure-of-Arrays (SoA) layout for branchless SIMD scoring.
 */
class FinOpsRouter {
public:
    explicit FinOpsRouter(double budget_limit = 10.0);

    [[nodiscard]] auto select_provider(const std::string& task_complexity,
                                       const std::string& priority = "balanced")
        -> std::string;

    void track_usage(const std::string& provider_name, int tokens);
    [[nodiscard]] auto current_spend() const -> double { return current_spend_; }

    void track_session_usage(const std::string& session_id,
                              const std::string& agent_id,
                              const std::string& model,
                              int input_tokens, int output_tokens);

    [[nodiscard]] auto session_cost(const std::string& session_id) const -> double;
    [[nodiscard]] auto check_budget(const std::string& session_id,
                                     double limit) const -> bool;

    void set_session_limit(size_t limit) { session_limit_ = limit; }
    [[nodiscard]] size_t get_session_limit() const { return session_limit_; }

private:
    void enforce_limits();

    [[maybe_unused]] double budget_limit_;
    double current_spend_{0.0};
    size_t session_limit_{100};

    // Hardware-aware Structure of Arrays (SoA) for SIMD vectorization
    std::vector<std::string> p_names_;
    std::vector<double> p_costs_;
    std::vector<int> p_latencies_;
    std::vector<double> p_reliabilities_;

    std::unordered_map<std::string, size_t> name_to_provider_; ///< O(1) provider lookup index
    std::map<std::string, std::vector<SessionUsageRecord>> session_usage_;
    std::list<std::string> session_order_;
    std::unordered_map<std::string, std::list<std::string>::iterator> session_index_;

    mutable std::mutex session_mutex_;           ///< P10: Thread-safe session tracking.
    std::atomic<size_t> rr_counter_{0};          ///< P10: Atomic round-robin counter.
};

} // namespace euxis::core
