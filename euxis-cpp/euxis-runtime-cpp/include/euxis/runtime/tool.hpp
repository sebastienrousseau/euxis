#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agent_session.hpp"

namespace euxis::runtime {

/**
 * @brief Simple tool interface for legacy or external tools.
 */
class ITool {
public:
    virtual ~ITool() = default;
    virtual auto declaration() const -> ToolDeclaration = 0;
    virtual auto execute(const nlohmann::json& args) -> nlohmann::json = 0;
};

} // namespace euxis::runtime
