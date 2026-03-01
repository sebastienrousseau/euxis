#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/a2a/message.hpp"

namespace euxis::a2a {

/// Task lifecycle states per A2A v0.2.
enum class TaskStatus : uint8_t {
    Pending,
    Active,
    Completed,
    Failed,
    Cancelled
};

/// Convert a TaskStatus to its string representation.
[[nodiscard]] constexpr auto status_string(TaskStatus s) -> std::string_view;

/// An A2A task representing a unit of work delegated to a remote agent.
struct A2ATask {
    std::string id;
    TaskStatus status = TaskStatus::Pending;
    std::vector<A2AMessage> messages;
    std::vector<Artifact> artifacts;
    std::vector<TaskStatus> history;
    std::string created_at;
    std::string updated_at;

    /// Serialise to JSON.
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Create a new task with a unique ID and optional initial message.
[[nodiscard]] auto create_task(std::string_view message = "") -> A2ATask;

/// Transition a task to a new status, enforcing the A2A state machine.
///
/// Valid transitions:
///   Pending  -> Active, Cancelled, Failed
///   Active   -> Completed, Failed, Cancelled
///   Completed, Failed, Cancelled are terminal (no transitions allowed).
///
/// On success, the old status is pushed to history and updated_at is refreshed.
[[nodiscard]] auto transition_task(A2ATask& task, TaskStatus new_status)
    -> std::expected<void, std::string>;

// ---------------------------------------------------------------------------
// constexpr implementation
// ---------------------------------------------------------------------------
constexpr auto status_string(TaskStatus s) -> std::string_view {
    switch (s) {
        case TaskStatus::Pending:   return "pending";
        case TaskStatus::Active:    return "active";
        case TaskStatus::Completed: return "completed";
        case TaskStatus::Failed:    return "failed";
        case TaskStatus::Cancelled: return "cancelled";
    }
    return "unknown";
}

} // namespace euxis::a2a
