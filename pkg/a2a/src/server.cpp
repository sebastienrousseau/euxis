#include "euxis/a2a/server.hpp"

#include <spdlog/spdlog.h>

namespace euxis::a2a {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
A2AServerHandler::A2AServerHandler(AgentCard card)
    : card_(std::move(card)) {
    spdlog::info("A2A server handler initialised for agent '{}'", card_.name);
}

// ---------------------------------------------------------------------------
// handle_request  (JSON-RPC style dispatcher)
// ---------------------------------------------------------------------------
auto A2AServerHandler::handle_request(std::string_view method,
                                       const nlohmann::json& params)
    -> nlohmann::json {

    spdlog::debug("A2A server handling method: {}", method);

    if (method == "agent/card") {
        return handle_agent_card();
    }
    if (method == "task/create") {
        return handle_task_create(params);
    }
    if (method == "task/get") {
        return handle_task_get(params);
    }
    if (method == "task/cancel") {
        return handle_task_cancel(params);
    }
    if (method == "capabilities/list") {
        return handle_capabilities_list();
    }

    spdlog::warn("A2A server: unknown method '{}'", method);
    return {
        {"error", {
            {"code", -32601},
            {"message", std::string("unknown method: ") + std::string(method)}
        }}
    };
}

// ---------------------------------------------------------------------------
// handle_agent_card
// ---------------------------------------------------------------------------
auto A2AServerHandler::handle_agent_card() -> nlohmann::json {
    return {{"result", card_.to_json()}};
}

// ---------------------------------------------------------------------------
// handle_task_create
// ---------------------------------------------------------------------------
auto A2AServerHandler::handle_task_create(const nlohmann::json& params)
    -> nlohmann::json {

    std::string initial_message;
    if (params.contains("message") && params["message"].is_string()) {
        initial_message = params["message"].get<std::string>();
    }

    auto task = create_task(initial_message);
    const auto id = task.id;  // copy before move
    tasks_.emplace(id, std::move(task));

    spdlog::info("A2A server created task: {}", id);
    return {{"result", tasks_.at(id).to_json()}};
}

// ---------------------------------------------------------------------------
// handle_task_get
// ---------------------------------------------------------------------------
auto A2AServerHandler::handle_task_get(const nlohmann::json& params)
    -> nlohmann::json {

    if (!params.contains("id") || !params["id"].is_string()) {
        return {
            {"error", {
                {"code", -32602},
                {"message", "missing or invalid 'id' parameter"}
            }}
        };
    }

    const auto id = params["id"].get<std::string>();
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return {
            {"error", {
                {"code", -32602},
                {"message", "task not found: " + id}
            }}
        };
    }

    return {{"result", it->second.to_json()}};
}

// ---------------------------------------------------------------------------
// handle_task_cancel
// ---------------------------------------------------------------------------
auto A2AServerHandler::handle_task_cancel(const nlohmann::json& params)
    -> nlohmann::json {

    if (!params.contains("id") || !params["id"].is_string()) {
        return {
            {"error", {
                {"code", -32602},
                {"message", "missing or invalid 'id' parameter"}
            }}
        };
    }

    const auto id = params["id"].get<std::string>();
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return {
            {"error", {
                {"code", -32602},
                {"message", "task not found: " + id}
            }}
        };
    }

    auto result = transition_task(it->second, TaskStatus::Cancelled);
    if (!result.has_value()) {
        return {
            {"error", {
                {"code", -32603},
                {"message", result.error()}
            }}
        };
    }

    spdlog::info("A2A server cancelled task: {}", id);
    return {{"result", it->second.to_json()}};
}

// ---------------------------------------------------------------------------
// handle_capabilities_list
// ---------------------------------------------------------------------------
auto A2AServerHandler::handle_capabilities_list() -> nlohmann::json {
    auto caps = nlohmann::json::array();
    for (const auto& cap : card_.capabilities) {
        nlohmann::json c;
        c["name"] = cap.name;
        c["description"] = cap.description;
        if (cap.input_schema.has_value()) {
            c["inputSchema"] = *cap.input_schema;
        }
        if (cap.output_schema.has_value()) {
            c["outputSchema"] = *cap.output_schema;
        }
        caps.push_back(std::move(c));
    }
    return {{"result", std::move(caps)}};
}

} // namespace euxis::a2a
