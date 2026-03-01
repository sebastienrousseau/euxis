#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

enum class AgentState {
    Idle,
    Ready,
    Running,
    Paused,
    Completed,
    Failed,
    Terminated,
};

auto state_to_string(AgentState s) -> std::string;
auto state_from_string(const std::string& s) -> AgentState;

/// Returns the set of states reachable from a given state.
auto valid_transitions(AgentState from) -> std::unordered_set<AgentState>;

struct Transition {
    std::string agent;
    AgentState from;
    AgentState to;
    std::string session;
    std::string timestamp; // ISO-8601
};

class LifecycleManager {
public:
    explicit LifecycleManager(std::filesystem::path data_dir);

    /// Get the current state of an agent.
    [[nodiscard]] auto get_state(const std::string& agent) const -> AgentState;

    /// Transition an agent to a new state.  Returns false if invalid.
    auto transition(const std::string& agent, AgentState new_state,
                    const std::string& session = {}) -> bool;

    /// Load state files from disk (data_dir/lifecycle/*.state).
    void load_from_disk();

    /// Persist current state for an agent.
    void persist_state(const std::string& agent) const;

    /// Get all known agents.
    [[nodiscard]] auto agents() const -> std::vector<std::string>;

    /// Get transition history for an agent.
    [[nodiscard]] auto history(const std::string& agent) const
        -> std::vector<Transition>;

private:
    std::filesystem::path data_dir_;
    std::filesystem::path lifecycle_dir_;
    std::filesystem::path transitions_file_;
    std::unordered_map<std::string, AgentState> states_;
    std::vector<Transition> transitions_;

    void append_transition(const Transition& t);
    static auto now_iso8601() -> std::string;
};

} // namespace euxis::runtime
