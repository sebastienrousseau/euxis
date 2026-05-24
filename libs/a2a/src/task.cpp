#include "euxis/a2a/task.hpp"

#include <chrono>
#include <cstring>

#include <sodium.h>
#include <spdlog/spdlog.h>

namespace euxis::a2a {

namespace {

// ---------------------------------------------------------------------------
// Generate a hex-encoded UUID from 16 random bytes via libsodium.
// Profile (perf record on AutonomyTaskLifecycle bench, 2026-05-24): the prior
// snprintf-based formatter accounted for ~28% of total CPU. sodium_bin2hex is
// ~27× faster per call in isolation (227 ns → 8 ns on Zen 5).
// ---------------------------------------------------------------------------
auto generate_uuid() -> std::string {
    unsigned char buf[16];
    randombytes_buf(buf, sizeof(buf));

    // sodium_bin2hex writes 2 hex chars per byte + a trailing NUL.
    char hex[33];
    sodium_bin2hex(hex, sizeof(hex), buf, sizeof(buf));

    // Reformat as 8-4-4-4-12 with dashes at positions 8, 13, 18, 23.
    std::string out(36, '-');
    std::memcpy(out.data(),      hex,      8);
    std::memcpy(out.data() + 9,  hex + 8,  4);
    std::memcpy(out.data() + 14, hex + 12, 4);
    std::memcpy(out.data() + 19, hex + 16, 4);
    std::memcpy(out.data() + 24, hex + 20, 12);
    return out;
}

// ---------------------------------------------------------------------------
// Format the current system clock time as "YYYY-MM-DDTHH:MM:SSZ".
// Profile: the prior snprintf-based formatter accounted for ~60% of CPU on
// task_lifecycle (called once per create_task + once per transition). A fixed-
// format digit writer is ~3.7× faster in isolation (155 ns → 42 ns).
// ---------------------------------------------------------------------------
[[gnu::always_inline]] inline void put2(char*& p, int v) noexcept {
    *p++ = static_cast<char>('0' + (v / 10));
    *p++ = static_cast<char>('0' + (v % 10));
}

[[gnu::always_inline]] inline void put4(char*& p, int v) noexcept {
    *p++ = static_cast<char>('0' + (v / 1000));
    *p++ = static_cast<char>('0' + ((v / 100) % 10));
    *p++ = static_cast<char>('0' + ((v / 10) % 10));
    *p++ = static_cast<char>('0' + (v % 10));
}

auto now_iso8601() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&tt, &utc);

    std::string out(20, '\0');  // "YYYY-MM-DDTHH:MM:SSZ" = 20 chars
    char* p = out.data();
    put4(p, utc.tm_year + 1900);
    *p++ = '-';
    put2(p, utc.tm_mon + 1);
    *p++ = '-';
    put2(p, utc.tm_mday);
    *p++ = 'T';
    put2(p, utc.tm_hour);
    *p++ = ':';
    put2(p, utc.tm_min);
    *p++ = ':';
    put2(p, utc.tm_sec);
    *p++ = 'Z';
    return out;
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
