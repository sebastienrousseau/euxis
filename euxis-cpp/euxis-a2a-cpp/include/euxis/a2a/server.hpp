/// @file
/// @brief A2A server handler for incoming agent requests.
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "agent_card.hpp"
#include "task.hpp"

namespace euxis::a2a {

/// @brief Dispatches and handles A2A JSON-RPC requests.
class A2AServerHandler {
public:
    /// @brief Construct handler with an agent identity card.
    explicit A2AServerHandler(AgentCard card);

    /// @brief Primary entry point for processing A2A requests.
    auto handle_request(std::string_view method, const nlohmann::json& params)
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
