#include "euxis/metrics/performance_collector.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <random>

#include <spdlog/spdlog.h>

namespace euxis::metrics {
namespace {

auto now_epoch() -> double {
    return std::chrono::duration<double>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

auto now_iso() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+00:00", &tm);
    return buf;
}

auto default_metrics_dir() -> std::filesystem::path {
    const char* home = std::getenv("EUXIS_HOME");
    if (home) return std::filesystem::path(home) / "data/runtime" / "metrics";
    const char* user_home = std::getenv("HOME");
    return std::filesystem::path(user_home ? user_home : "/tmp") /
           ".euxis" / "data/runtime" / "metrics";
}

} // namespace

PerformanceMetricsCollector::PerformanceMetricsCollector(
    const std::filesystem::path& metrics_dir)
    : metrics_dir_(metrics_dir.empty() ? default_metrics_dir() : metrics_dir) {
    events_file_ = metrics_dir_ / "events.jsonl";
    sessions_file_ = metrics_dir_ / "sessions.jsonl";
    std::filesystem::create_directories(metrics_dir_);
}

auto PerformanceMetricsCollector::generate_correlation_id() -> std::string {
    std::random_device rd;
    std::uniform_int_distribution<uint64_t> dist;
    auto val = dist(rd);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "metrics-%012lx",
                  static_cast<unsigned long>(val & 0xFFFFFFFFFFFFULL));
    return buf;
}

void PerformanceMetricsCollector::emit_event(
    const std::string& event_type, const nlohmann::json& properties) {
    nlohmann::json event = {
        {"event_type", event_type},
        {"timestamp", now_iso()},
        {"properties", properties},
    };
    std::lock_guard lock(mutex_);
    std::ofstream f(events_file_, std::ios::app);
    f << event.dump() << '\n';
}

auto PerformanceMetricsCollector::task_started(
    const std::string& agent_id, const std::string& session_id,
    const TaskStartContext& ctx) -> std::string {
    auto cid = generate_correlation_id();
    {
        std::lock_guard lock(mutex_);
        active_sessions_[session_id] = {
            .agent_id = agent_id,
            .start_time = now_epoch(),
            .correlation_id = cid,
            .task_type = ctx.task_type,
            .priority = ctx.priority,
        };
    }

    nlohmann::json props = {
        {"correlation_id", cid},
        {"agent_id", agent_id},
        {"session_id", session_id},
        {"task_type", ctx.task_type},
        {"priority", ctx.priority},
    };
    if (ctx.task_complexity) props["task_complexity"] = *ctx.task_complexity;
    if (ctx.parent_session) props["parent_session"] = *ctx.parent_session;

    emit_event("Agent:TaskStarted", props);
    return cid;
}

void PerformanceMetricsCollector::task_completed(
    const std::string& session_id, const TaskCompletionContext& ctx) {
    SessionData session_data;
    {
        std::lock_guard lock(mutex_);
        auto it = active_sessions_.find(session_id);
        if (it == active_sessions_.end()) return;
        session_data = it->second;
        active_sessions_.erase(it);
    }

    auto duration_ms =
        static_cast<int>((now_epoch() - session_data.start_time) * 1000);

    nlohmann::json props = {
        {"correlation_id", session_data.correlation_id},
        {"agent_id", session_data.agent_id},
        {"session_id", session_id},
        {"duration_ms", duration_ms},
        {"status", ctx.status},
        {"artifacts_created", ctx.artifacts_created},
        {"cortex_operations", ctx.cortex_operations},
        {"tool_calls_count", ctx.tool_calls_count},
        {"handoff_required", ctx.handoff_required},
        {"delegation_count", ctx.delegation_count},
    };
    emit_event("Agent:TaskCompleted", props);

    nlohmann::json summary = {
        {"session_id", session_id},
        {"agent_id", session_data.agent_id},
        {"duration_ms", duration_ms},
        {"status", ctx.status},
        {"task_type", session_data.task_type},
        {"priority", session_data.priority},
        {"completed_at", now_iso()},
    };
    {
        std::lock_guard lock(mutex_);
        std::ofstream f(sessions_file_, std::ios::app);
        f << summary.dump() << '\n';
    }
}

void PerformanceMetricsCollector::task_failed(
    const std::string& session_id, const TaskFailureContext& ctx) {
    SessionData session_data;
    {
        std::lock_guard lock(mutex_);
        auto it = active_sessions_.find(session_id);
        if (it == active_sessions_.end()) return;
        session_data = it->second;
        active_sessions_.erase(it);
    }

    auto duration_ms =
        static_cast<int>((now_epoch() - session_data.start_time) * 1000);

    nlohmann::json props = {
        {"correlation_id", session_data.correlation_id},
        {"agent_id", session_data.agent_id},
        {"session_id", session_id},
        {"duration_ms", duration_ms},
        {"failure_reason", ctx.failure_reason},
        {"error_category", ctx.error_category},
        {"partial_completion", ctx.partial_completion},
        {"reflexion_generated", ctx.reflexion_generated},
    };
    emit_event("Agent:TaskFailed", props);
}

auto PerformanceMetricsCollector::delegation_started(
    const std::string& delegating_agent, const std::string& target_agent,
    const std::string& reason, const std::string& priority) -> std::string {
    auto cid = generate_correlation_id();
    nlohmann::json props = {
        {"correlation_id", cid},
        {"delegating_agent", delegating_agent},
        {"target_agent", target_agent},
        {"delegation_reason", reason},
        {"priority", priority},
    };
    emit_event("Agent:DelegationStarted", props);
    return cid;
}

void PerformanceMetricsCollector::delegation_completed(
    const std::string& correlation_id, const std::string& delegating_agent,
    const DelegationContext& ctx) {
    nlohmann::json props = {
        {"correlation_id", correlation_id},
        {"delegating_agent", delegating_agent},
        {"target_agent", ctx.target_agent},
        {"total_duration_ms", ctx.total_duration_ms},
        {"handoff_successful", ctx.handoff_successful},
    };
    if (ctx.quality_score) props["quality_score"] = *ctx.quality_score;
    emit_event("Agent:DelegationCompleted", props);
}

void PerformanceMetricsCollector::tool_execution(
    const std::string& agent_id, const std::string& tool_name,
    const ToolExecutionContext& ctx) {
    auto cid = ctx.correlation_id.value_or(generate_correlation_id());
    nlohmann::json props = {
        {"correlation_id", cid},
        {"agent_id", agent_id},
        {"tool_name", tool_name},
        {"execution_duration_ms", ctx.execution_duration_ms},
        {"success", ctx.success},
    };
    if (ctx.retries > 0) props["retries"] = ctx.retries;
    emit_event("Agent:ToolExecution", props);
}

void PerformanceMetricsCollector::memory_operation(
    const std::string& agent_id, const std::string& operation,
    const std::string& memory_type, int operation_duration_ms,
    const std::string& correlation_id) {
    auto cid = correlation_id.empty() ? generate_correlation_id() : correlation_id;
    nlohmann::json props = {
        {"correlation_id", cid},
        {"agent_id", agent_id},
        {"operation", operation},
        {"memory_type", memory_type},
        {"operation_duration_ms", operation_duration_ms},
    };
    emit_event("Agent:MemoryOperation", props);
}

void PerformanceMetricsCollector::reflexion_generated(
    const std::string& agent_id, const std::string& failure_category,
    bool contraindication_stored, const std::string& correlation_id) {
    auto cid = correlation_id.empty() ? generate_correlation_id() : correlation_id;
    nlohmann::json props = {
        {"correlation_id", cid},
        {"agent_id", agent_id},
        {"failure_category", failure_category},
        {"contraindication_stored", contraindication_stored},
    };
    emit_event("Agent:ReflexionGenerated", props);
}

auto PerformanceMetricsCollector::cleanup_stale_sessions(
    int timeout_seconds) -> int {
    auto current_time = now_epoch();
    std::vector<std::string> stale;
    {
        std::lock_guard lock(mutex_);
        for (const auto& [sid, data] : active_sessions_) {
            if (current_time - data.start_time > timeout_seconds) {
                stale.push_back(sid);
            }
        }
    }
    for (const auto& sid : stale) {
        task_failed(sid, {.failure_reason = "Session timeout",
                          .error_category = "timeout"});
    }
    return static_cast<int>(stale.size());
}

} // namespace euxis::metrics
