/// @file
/// @brief Management of agent state and lifecycle transitions.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

/// @brief qualitative states an agent can occupy.
enum class AgentState {
    Idle,
    Ready,
    Running,
    Paused,
    Completed,
    Failed,
    Terminated
};

/// @brief Record of a state change event.
struct Transition {
    std::string agent;
    AgentState from;
    AgentState to;
    std::string session;
    std::string timestamp;
};

/// @brief manages and persists the current state and transition history of agents.
class LifecycleManager {
public:
    /// @brief Construct manager targeting a data directory.
    explicit LifecycleManager(std::filesystem::path data_dir);

    /// @brief Get the current state of an agent.
    auto get_state(const std::string& agent) const -> AgentState;
    
    /// @brief Transition an agent to a new state. Returns false if invalid.
    auto transition(const std::string& agent, AgentState new_state,
                    const std::string& session = "") -> bool;

    /// @brief Refresh state data from the filesystem.
    void load_from_disk();
    
    /// @brief List all agents tracked by the manager.
    auto agents() const -> std::vector<std::string>;
    
    /// @brief Retrieve the transition history for an agent.
    auto history(const std::string& agent) const -> std::vector<Transition>;

private:
    std::filesystem::path data_dir_;
    std::filesystem::path lifecycle_dir_;
    std::filesystem::path transitions_file_;
    std::map<std::string, AgentState> states_;
    std::vector<Transition> transitions_;

    void persist_state(const std::string& agent) const;
    void append_transition(const Transition& t);
    static auto now_iso8601() -> std::string;
};

/// @brief Convert AgentState to string.
auto state_to_string(AgentState s) -> std::string;

/// @brief Parse AgentState from string.
auto state_from_string(const std::string& s) -> AgentState;

/// @brief Get set of valid states reachable from a given state.
auto valid_transitions(AgentState from) -> std::unordered_set<AgentState>;

} // namespace euxis::runtime
