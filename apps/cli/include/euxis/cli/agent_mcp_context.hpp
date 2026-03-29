#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace euxis::cli {

/// Thread-safe context for sharing findings between agents during fleet execution.
class AgentMcpContext {
public:
    /// Publish findings for an agent after completion.
    void publish_agent_findings(const std::string& agent_id,
                                 const std::string& verdict,
                                 const std::vector<std::string>& findings,
                                 double duration_ms);

    /// Query findings from a specific agent.
    [[nodiscard]] auto query_agent(const std::string& agent_id) const -> std::optional<nlohmann::json>;

    /// Get list of agents that have published findings.
    [[nodiscard]] auto available_agents() const -> std::vector<std::string>;

    /// Build a context summary of all prior agent findings for injection into prompts.
    [[nodiscard]] auto build_context_summary() const -> std::string;

    /// Check if an agent has published findings.
    [[nodiscard]] auto has_agent(const std::string& agent_id) const -> bool;

    /// Get the number of agents that have published.
    [[nodiscard]] auto agent_count() const -> size_t;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, nlohmann::json> agent_findings_;
    std::vector<std::string> agent_order_;  // Preserves publication order
};

} // namespace euxis::cli
