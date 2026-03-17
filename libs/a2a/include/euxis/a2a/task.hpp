/// @file
/// @brief A2A task management and lifecycle.
#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <euxis/a2a/message.hpp>

namespace euxis::a2a {

/// @brief Status of an A2A task.
enum class TaskStatus {
    Pending,
    Active,
    Completed,
    Failed,
    Cancelled
};

/// @brief Convert TaskStatus to string.
[[nodiscard]] constexpr auto status_string(TaskStatus s) -> std::string_view {
    switch (s) {
        case TaskStatus::Pending:   return "pending";
        case TaskStatus::Active:    return "active";
        case TaskStatus::Completed: return "completed";
        case TaskStatus::Failed:    return "failed";
        case TaskStatus::Cancelled: return "cancelled";
    }
    return "unknown";
}

/// @brief A multi-step task involving one or more agents.
struct A2ATask {
    std::string id;
    TaskStatus status;
    std::string created_at;
    std::string updated_at;
    std::vector<A2AMessage> messages;
    std::vector<Artifact> artifacts;
    std::vector<TaskStatus> history;

    [[nodiscard]] nlohmann::json to_json() const;
};

/// @brief Factory function to create a new task.
[[nodiscard]] auto create_task(std::string_view message = "") -> A2ATask;

/// @brief Move a task to a new state.
auto transition_task(A2ATask& task, TaskStatus new_status)
    -> std::expected<void, std::string>;

} // namespace euxis::a2a
