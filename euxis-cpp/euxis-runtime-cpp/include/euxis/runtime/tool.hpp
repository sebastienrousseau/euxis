#pragma once

#include <expected>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

struct ToolDeclaration {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
};

using ToolHandler = std::function<std::expected<nlohmann::json, std::string>(
    const nlohmann::json&)>;

/// Abstract tool registry interface.
class IToolRegistry {
public:
    virtual ~IToolRegistry() = default;

    virtual void register_tool(ToolDeclaration decl, ToolHandler handler) = 0;

    virtual auto invoke(const std::string& name, const nlohmann::json& input)
        -> std::expected<nlohmann::json, std::string> = 0;

    virtual auto list_tools() const -> std::vector<ToolDeclaration> = 0;

    virtual void remove_tool(const std::string& name) = 0;
};

} // namespace euxis::runtime
