#include "euxis/cli/agent_mcp_context.hpp"

#include <sstream>

namespace euxis::cli {

void AgentMcpContext::publish_agent_findings(const std::string& agent_id,
                                              const std::string& verdict,
                                              const std::vector<std::string>& findings,
                                              double duration_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json entry;
    entry["agent_id"] = agent_id;
    entry["verdict"] = verdict;
    entry["findings"] = findings;
    entry["duration_ms"] = duration_ms;

    if (agent_findings_.find(agent_id) == agent_findings_.end()) {
        agent_order_.push_back(agent_id);
    }
    agent_findings_[agent_id] = std::move(entry);
}

auto AgentMcpContext::query_agent(const std::string& agent_id) const -> std::optional<nlohmann::json> {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = agent_findings_.find(agent_id);
    if (it == agent_findings_.end()) return std::nullopt;
    return it->second;
}

auto AgentMcpContext::available_agents() const -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(mutex_);
    return agent_order_;
}

auto AgentMcpContext::build_context_summary() const -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);
    if (agent_findings_.empty()) return "";

    std::ostringstream ss;
    ss << "## Prior Agent Findings\n\n";
    for (const auto& agent_id : agent_order_) {
        auto it = agent_findings_.find(agent_id);
        if (it == agent_findings_.end()) continue;

        const auto& entry = it->second;
        ss << "### " << agent_id << " (" << entry.value("verdict", "?") << ")\n";

        if (entry.contains("findings") && entry["findings"].is_array()) {
            for (const auto& f : entry["findings"]) {
                ss << "- " << f.get<std::string>() << "\n";
            }
        }
        ss << "\n";
    }
    return ss.str();
}

auto AgentMcpContext::has_agent(const std::string& agent_id) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return agent_findings_.count(agent_id) > 0;
}

auto AgentMcpContext::agent_count() const -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return agent_findings_.size();
}

} // namespace euxis::cli
