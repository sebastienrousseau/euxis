#pragma once

#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::core {

struct AgentExecutionRequest {
    std::string agent_id;
    std::string function_name;
    nlohmann::json payload;
    double timeout_seconds{30.0};
};

struct AgentExecutionResult {
    bool ok{false};
    nlohmann::json output;
    std::optional<std::string> error;
    int latency_ms{0};
};

struct SwarmTask {
    std::string agent_id;
    std::string task_template;
    std::string status{"pending"};
    std::optional<std::string> result;
    std::string run_id;
    std::string complexity{"medium"};
};

} // namespace euxis::core
