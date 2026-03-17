#pragma once

#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::core {

/**
 * @brief Structured request for a single agent function call.
 */
struct AgentExecutionRequest {
    std::string agent_id;           ///< Target agent identifier.
    std::string function_name;      ///< Specific capability/function to invoke.
    nlohmann::json payload;         ///< Arguments passed to the function.
    double timeout_seconds{30.0};   ///< Maximum execution time before failure.
};

/**
 * @brief Outcome of an agent execution request.
 */
struct AgentExecutionResult {
    bool ok{false};                 ///< True if execution completed successfully.
    nlohmann::json output;          ///< Returned data from the agent.
    std::optional<std::string> error; ///< Descriptive error message on failure.
    int latency_ms{0};              ///< Total round-trip time in milliseconds.
};

/**
 * @brief Represents a granular unit of work within a Swarm playbook.
 */
struct SwarmTask {
    std::string agent_id;           ///< Agent assigned to this task.
    std::string task_template;      ///< Raw prompt/template for the task.
    std::string status{"pending"};  ///< Lifecycle state: pending, running, completed, failed.
    std::optional<std::string> result; ///< Final output string from the task.
    std::string run_id;             ///< Unique ID for this specific task execution.
    std::string complexity{"medium"}; ///< Routing hint: low, medium, high.
};

} // namespace euxis::core
