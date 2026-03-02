#include "euxis/runtime/tool.hpp"

#include <algorithm>
#include <unordered_map>

namespace euxis::runtime {

class InMemoryToolRegistry : public IToolRegistry {
public:
    void register_tool(ToolDeclaration decl, ToolHandler handler) override {
        std::string name = decl.name;
        tools_[name] = {std::move(decl), std::move(handler)};
    }

    auto invoke(const std::string& name, const nlohmann::json& input)
        -> std::expected<nlohmann::json, std::string> override {
        auto it = tools_.find(name);
        if (it == tools_.end())
            return std::unexpected("Tool not found: " + name);
        return it->second.handler(input);
    }

    auto list_tools() const -> std::vector<ToolDeclaration> override {
        std::vector<ToolDeclaration> result;
        result.reserve(tools_.size());
        for (const auto& [name, entry] : tools_)
            result.push_back(entry.decl);
        return result;
    }

    void remove_tool(const std::string& name) override {
        tools_.erase(name);
    }

private:
    struct ToolEntry {
        ToolDeclaration decl;
        ToolHandler handler;
    };
    std::unordered_map<std::string, ToolEntry> tools_;
};

auto make_tool_registry() -> std::unique_ptr<IToolRegistry> {
    return std::make_unique<InMemoryToolRegistry>();
}

} // namespace euxis::runtime
