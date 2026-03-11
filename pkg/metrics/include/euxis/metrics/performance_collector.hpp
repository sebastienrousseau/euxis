/// @file
/// @brief Collection of detailed agent and task performance metrics.
#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::metrics {

/// @brief Context provided when starting a new task.
struct TaskStartContext {
    std::string task_type{"general"};
    std::string priority{"medium"};
    std::optional<std::string> task_complexity;
    std::optional<std::string> parent_session;
};

/// @brief Context provided upon successful task completion.
struct TaskCompletionContext {
    std::string status{"success"};
    int artifacts_created{0};
    int cortex_operations{0};
    int tool_calls_count{0};
    bool handoff_required{false};
    int delegation_count{0};
};

/// @brief Context provided when a task fails.
struct TaskFailureContext {
    std::string failure_reason{"unknown"};
    std::string error_category{"unknown"};
    double partial_completion{0.0};
    bool reflexion_generated{false};
};

/// @brief Context for agent-to-agent delegation events.
struct DelegationContext {
    std::string target_agent{"unknown"};
    int total_duration_ms{0};
    bool handoff_successful{true};
    std::optional<double> quality_score;
};

/// @brief Context for individual tool execution metrics.
struct ToolExecutionContext {
    int execution_duration_ms{0};
    bool success{true};
    int retries{0};
    std::optional<std::string> correlation_id;
};

/// @brief Collector that aggregates and persists performance and behavioral metrics.
class PerformanceMetricsCollector {
public:
    /// @brief Construct collector targeting a specific directory.
    explicit PerformanceMetricsCollector(
        const std::filesystem::path& metrics_dir = {});

    /// @brief Generate a unique correlation ID for event linking.
    auto generate_correlation_id() -> std::string;

    /// @brief Emit a raw event to the metrics stream.
    void emit_event(const std::string& event_type,
                    const nlohmann::json& properties);

    /// @brief record the start of a task.
    auto task_started(const std::string& agent_id, const std::string& session_id,
                      const TaskStartContext& ctx = {}) -> std::string;

    /// @brief record successful completion of a task.
    void task_completed(const std::string& session_id,
                        const TaskCompletionContext& ctx = {});

    /// @brief record a task failure.
    void task_failed(const std::string& session_id,
                     const TaskFailureContext& ctx = {});

    /// @brief record start of an agent delegation.
    auto delegation_started(const std::string& delegating_agent,
                            const std::string& target_agent,
                            const std::string& reason = "",
                            const std::string& priority = "medium") -> std::string;

    /// @brief record completion of an agent delegation.
    void delegation_completed(const std::string& correlation_id,
                              const std::string& delegating_agent,
                              const DelegationContext& ctx = {});

    /// @brief record tool execution metrics.
    void tool_execution(const std::string& agent_id,
                        const std::string& tool_name,
                        const ToolExecutionContext& ctx = {});

    /// @brief record memory/storage operation metrics.
    void memory_operation(const std::string& agent_id,
                          const std::string& operation,
                          const std::string& memory_type,
                          int operation_duration_ms,
                          const std::string& correlation_id = "");

    /// @brief record a self-correction/reflexion event.
    void reflexion_generated(const std::string& agent_id,
                             const std::string& failure_category,
                             bool contraindication_stored,
                             const std::string& correlation_id = "");

    /// @brief purge old session data from memory.
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
    std::map<std::string, SessionData> active_sessions_;
};

} // namespace euxis::metrics
