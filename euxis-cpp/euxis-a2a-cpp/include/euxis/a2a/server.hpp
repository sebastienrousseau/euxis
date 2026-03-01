#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "euxis/a2a/agent_card.hpp"
#include "euxis/a2a/task.hpp"

namespace euxis::a2a {

/// Server-side handler for incoming A2A JSON-RPC requests.
///
/// Dispatches the following methods:
///   "agent/card"          -> returns this agent's card
///   "task/create"         -> creates a new task (with optional initial message)
///   "task/get"            -> retrieves a task by ID
///   "task/cancel"         -> transitions a task to Cancelled
///   "capabilities/list"   -> returns the agent's capabilities
///   Unknown methods       -> returns a JSON error object
class A2AServerHandler {
public:
    explicit A2AServerHandler(AgentCard card);

    /// Handle an incoming JSON-RPC request and return the response payload.
    [[nodiscard]] auto handle_request(std::string_view method, const nlohmann::json& params)
        -> nlohmann::json;

private:
    AgentCard card_;
    std::unordered_map<std::string, A2ATask> tasks_;

    auto handle_agent_card() -> nlohmann::json;
    auto handle_task_create(const nlohmann::json& params) -> nlohmann::json;
    auto handle_task_get(const nlohmann::json& params) -> nlohmann::json;
    auto handle_task_cancel(const nlohmann::json& params) -> nlohmann::json;
    auto handle_capabilities_list() -> nlohmann::json;
};

} // namespace euxis::a2a
