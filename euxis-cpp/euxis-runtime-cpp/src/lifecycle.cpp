#include "euxis/runtime/lifecycle.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace euxis::runtime {

auto state_to_string(AgentState s) -> std::string {
    switch (s) {
        case AgentState::Idle: return "idle";
        case AgentState::Ready: return "ready";
        case AgentState::Running: return "running";
        case AgentState::Paused: return "paused";
        case AgentState::Completed: return "completed";
        case AgentState::Failed: return "failed";
        case AgentState::Terminated: return "terminated";
    }
    return "idle";
}

auto state_from_string(const std::string& s) -> AgentState {
    if (s == "ready") return AgentState::Ready;
    if (s == "running") return AgentState::Running;
    if (s == "paused") return AgentState::Paused;
    if (s == "completed") return AgentState::Completed;
    if (s == "failed") return AgentState::Failed;
    if (s == "terminated") return AgentState::Terminated;
    return AgentState::Idle;
}

auto valid_transitions(AgentState from) -> std::unordered_set<AgentState> {
    switch (from) {
        case AgentState::Idle:
            return {AgentState::Ready, AgentState::Terminated};
        case AgentState::Ready:
            return {AgentState::Running, AgentState::Terminated};
        case AgentState::Running:
            return {AgentState::Paused, AgentState::Completed,
                    AgentState::Failed, AgentState::Terminated};
        case AgentState::Paused:
            return {AgentState::Running, AgentState::Terminated};
        case AgentState::Completed:
            return {AgentState::Idle};
        case AgentState::Failed:
            return {AgentState::Idle, AgentState::Terminated};
        case AgentState::Terminated:
            return {};
    }
    return {};
}

LifecycleManager::LifecycleManager(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)),
      lifecycle_dir_(data_dir_ / "lifecycle"),
      transitions_file_(lifecycle_dir_ / "transitions.jsonl") {
    std::filesystem::create_directories(lifecycle_dir_);
}

auto LifecycleManager::get_state(const std::string& agent) const
    -> AgentState {
    auto it = states_.find(agent);
    return it != states_.end() ? it->second : AgentState::Idle;
}

auto LifecycleManager::transition(const std::string& agent,
                                   AgentState new_state,
                                   const std::string& session) -> bool {
    auto current = get_state(agent);
    auto allowed = valid_transitions(current);
    if (!allowed.contains(new_state)) {
        return false;
    }

    auto old_state = current;
    states_[agent] = new_state;

    Transition t{
        .agent = agent,
        .from = old_state,
        .to = new_state,
        .session = session.empty() ? "default" : session,
        .timestamp = now_iso8601(),
    };
    transitions_.push_back(t);
    append_transition(t);
    persist_state(agent);
    return true;
}

void LifecycleManager::load_from_disk() {
    if (!std::filesystem::exists(lifecycle_dir_)) return;

    for (const auto& entry :
         std::filesystem::directory_iterator(lifecycle_dir_)) {
        if (entry.path().extension() == ".state") {
            auto agent = entry.path().stem().string();
            std::ifstream f(entry.path());
            std::string content;
            std::getline(f, content);
            // Trim whitespace
            auto start = content.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                content = content.substr(start);
                auto end = content.find_last_not_of(" \t\r\n");
                content = content.substr(0, end + 1);
            }
            states_[agent] = state_from_string(content);
        }
    }

    // Load transition history
    if (std::filesystem::exists(transitions_file_)) {
        std::ifstream f(transitions_file_);
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            try {
                auto j = nlohmann::json::parse(line);
                transitions_.push_back({
                    .agent = j.value("agent", ""),
                    .from = state_from_string(j.value("from", "idle")),
                    .to = state_from_string(j.value("state", "idle")),
                    .session = j.value("session", ""),
                    .timestamp = j.value("ts", ""),
                });
            } catch (...) {
                continue;
            }
        }
    }
}

void LifecycleManager::persist_state(const std::string& agent) const {
    auto path = lifecycle_dir_ / (agent + ".state");
    std::ofstream f(path);
    f << state_to_string(states_.at(agent));
}

auto LifecycleManager::agents() const -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(states_.size());
    for (const auto& [name, _] : states_) {
        result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

auto LifecycleManager::history(const std::string& agent) const
    -> std::vector<Transition> {
    std::vector<Transition> result;
    for (const auto& t : transitions_) {
        if (t.agent == agent) result.push_back(t);
    }
    return result;
}

void LifecycleManager::append_transition(const Transition& t) {
    nlohmann::json j = {
        {"ts", t.timestamp},
        {"agent", t.agent},
        {"from", state_to_string(t.from)},
        {"state", state_to_string(t.to)},
        {"session", t.session},
    };
    std::ofstream f(transitions_file_, std::ios::app);
    f << j.dump() << '\n';
}

auto LifecycleManager::now_iso8601() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << "Z";
    return oss.str();
}

} // namespace euxis::runtime
