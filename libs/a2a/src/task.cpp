#include "euxis/a2a/task.hpp"

#include <chrono>
#include <cstdio>
#include <format>

#include <sodium.h>
#include <spdlog/spdlog.h>

namespace euxis::a2a {

namespace {

// ---------------------------------------------------------------------------
// Generate a hex-encoded UUID from 16 random bytes via libsodium.
// ---------------------------------------------------------------------------
auto generate_uuid() -> std::string {
    unsigned char buf[16];
    randombytes_buf(buf, sizeof(buf));

    // Format as 8-4-4-4-12 hex string
    char hex[37];
    std::snprintf(hex, sizeof(hex),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        buf[0], buf[1], buf[2], buf[3],
        buf[4], buf[5],
        buf[6], buf[7],
        buf[8], buf[9],
        buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
    return std::string(hex);
}

// ---------------------------------------------------------------------------
// Format the current system clock time as "YYYY-MM-DDTHH:MM:SSZ".
// ---------------------------------------------------------------------------
auto now_iso8601() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&tt, &utc);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
        utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
        utc.tm_hour, utc.tm_min, utc.tm_sec);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Check whether a status is terminal (no outgoing transitions).
// ---------------------------------------------------------------------------
auto is_terminal(TaskStatus s) -> bool {
    return s == TaskStatus::Completed
        || s == TaskStatus::Failed
        || s == TaskStatus::Cancelled;
}

// ---------------------------------------------------------------------------
// Check whether a transition from `from` to `to` is valid.
// ---------------------------------------------------------------------------
auto is_valid_transition(TaskStatus from, TaskStatus to) -> bool {
    switch (from) {
        case TaskStatus::Pending:
            return to == TaskStatus::Active
                || to == TaskStatus::Cancelled
                || to == TaskStatus::Failed;
        case TaskStatus::Active:
            return to == TaskStatus::Completed
                || to == TaskStatus::Failed
                || to == TaskStatus::Cancelled;
        case TaskStatus::Completed:
        case TaskStatus::Failed:
        case TaskStatus::Cancelled:
            return false;
    }
    return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// A2ATask::to_json
// ---------------------------------------------------------------------------
nlohmann::json A2ATask::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["status"] = std::string(status_string(status));
    j["createdAt"] = created_at;
    j["updatedAt"] = updated_at;

    auto msgs = nlohmann::json::array();
    for (const auto& msg : messages) {
        msgs.push_back(msg.to_json());
    }
    j["messages"] = std::move(msgs);

    auto arts = nlohmann::json::array();
    for (const auto& art : artifacts) {
        arts.push_back(art.to_json());
    }
    j["artifacts"] = std::move(arts);

    auto hist = nlohmann::json::array();
    for (const auto& h : history) {
        hist.push_back(std::string(status_string(h)));
    }
    j["history"] = std::move(hist);

    return j;
}

// ---------------------------------------------------------------------------
// create_task
// ---------------------------------------------------------------------------
auto create_task(std::string_view message) -> A2ATask {
    A2ATask task;
    task.id = generate_uuid();
    task.status = TaskStatus::Pending;
    task.created_at = now_iso8601();
    task.updated_at = task.created_at;

    if (!message.empty()) {
        A2AMessage msg;
        msg.role = "user";
        msg.created_at = task.created_at;

        ContentPart part;
        part.type = "text";
        part.content = std::string(message);
        msg.parts.push_back(std::move(part));

        task.messages.push_back(std::move(msg));
    }

    spdlog::debug("A2A task created: id={}", task.id);
    return task;
}

// ---------------------------------------------------------------------------
// transition_task
// ---------------------------------------------------------------------------
auto transition_task(A2ATask& task, TaskStatus new_status)
    -> std::expected<void, std::string> {

    if (is_terminal(task.status)) {
        return std::unexpected(std::format(
            "cannot transition from terminal state '{}'",
            status_string(task.status)));
    }

    if (!is_valid_transition(task.status, new_status)) {
        return std::unexpected(std::format(
            "invalid transition from '{}' to '{}'",
            status_string(task.status),
            status_string(new_status)));
    }

    task.history.push_back(task.status);
    task.status = new_status;
    task.updated_at = now_iso8601();

    spdlog::debug("A2A task {} transitioned to {}", task.id,
                  status_string(new_status));
    return {};
}

} // namespace euxis::a2a
