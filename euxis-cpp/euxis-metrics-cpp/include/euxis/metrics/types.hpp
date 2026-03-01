#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::metrics {

struct TaskStartContext {
    std::string task_type{"user_request"};
    std::string priority{"P2"};
    std::optional<std::string> task_complexity;
    std::optional<std::string> parent_session;
};

struct TaskCompletionContext {
    std::string status{"SUCCESS"};
    int artifacts_created{0};
    int cortex_operations{0};
    int tool_calls_count{0};
    bool handoff_required{false};
    int delegation_count{0};
};

struct TaskFailureContext {
    std::string failure_reason;
    std::string error_category{"internal_error"};
    bool partial_completion{false};
    bool reflexion_generated{false};
};

struct DelegationContext {
    std::string target_agent;
    int total_duration_ms{0};
    bool handoff_successful{true};
    std::optional<double> quality_score;
};

struct ToolExecutionContext {
    int execution_duration_ms{0};
    bool success{true};
    int retries{0};
    std::optional<std::string> correlation_id;
};

struct MetricEvent {
    std::string event_type;
    double timestamp;
    nlohmann::json properties;
};

} // namespace euxis::metrics
