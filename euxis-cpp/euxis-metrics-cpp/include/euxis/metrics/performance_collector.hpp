#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "types.hpp"

namespace euxis::metrics {

class PerformanceMetricsCollector {
public:
    explicit PerformanceMetricsCollector(
        const std::filesystem::path& metrics_dir = {});

    auto task_started(const std::string& agent_id,
                      const std::string& session_id,
                      const TaskStartContext& ctx = {}) -> std::string;

    void task_completed(const std::string& session_id,
                        const TaskCompletionContext& ctx = {});

    void task_failed(const std::string& session_id,
                     const TaskFailureContext& ctx = {});

    auto delegation_started(const std::string& delegating_agent,
                            const std::string& target_agent,
                            const std::string& reason = "scope_boundary",
                            const std::string& priority = "P2") -> std::string;

    void delegation_completed(const std::string& correlation_id,
                              const std::string& delegating_agent,
                              const DelegationContext& ctx = {});

    void tool_execution(const std::string& agent_id,
                        const std::string& tool_name,
                        const ToolExecutionContext& ctx = {});

    void memory_operation(const std::string& agent_id,
                          const std::string& operation,
                          const std::string& memory_type,
                          int operation_duration_ms,
                          const std::string& correlation_id = {});

    void reflexion_generated(const std::string& agent_id,
                             const std::string& failure_category,
                             bool contraindication_stored = true,
                             const std::string& correlation_id = {});

    auto cleanup_stale_sessions(int timeout_seconds = 3600) -> int;

private:
    struct SessionData {
        std::string agent_id;
        double start_time;
        std::string correlation_id;
        std::string task_type;
        std::string priority;
    };

    std::filesystem::path metrics_dir_;
    std::filesystem::path events_file_;
    std::filesystem::path sessions_file_;
    std::mutex mutex_;
    std::unordered_map<std::string, SessionData> active_sessions_;

    auto generate_correlation_id() -> std::string;
    void emit_event(const std::string& event_type,
                    const nlohmann::json& properties);
};

} // namespace euxis::metrics
