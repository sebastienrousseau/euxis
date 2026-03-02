#include <gtest/gtest.h>

#include "euxis/runtime/tool.hpp"

namespace euxis::runtime {

auto make_tool_registry() -> std::unique_ptr<IToolRegistry>;

namespace {

TEST(ToolRegistryTest, RegisterAndInvoke) {
    auto reg = make_tool_registry();
    reg->register_tool(
        {.name = "echo", .description = "Echoes input", .input_schema = {}},
        [](const nlohmann::json& input) -> std::expected<nlohmann::json, std::string> {
            return input;
        }
    );

    auto result = reg->invoke("echo", {{"msg", "hello"}});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)["msg"], "hello");
}

TEST(ToolRegistryTest, InvokeMissingToolFails) {
    auto reg = make_tool_registry();
    auto result = reg->invoke("nonexistent", {});
    EXPECT_FALSE(result.has_value());
}

TEST(ToolRegistryTest, ListTools) {
    auto reg = make_tool_registry();
    reg->register_tool(
        {.name = "tool_a", .description = "First tool", .input_schema = {}},
        [](const nlohmann::json&) { return nlohmann::json{}; }
    );
    reg->register_tool(
        {.name = "tool_b", .description = "Second tool", .input_schema = {}},
        [](const nlohmann::json&) { return nlohmann::json{}; }
    );

    auto tools = reg->list_tools();
    EXPECT_EQ(tools.size(), 2u);
}

TEST(ToolRegistryTest, RemoveTool) {
    auto reg = make_tool_registry();
    reg->register_tool(
        {.name = "removable", .description = "Will be removed", .input_schema = {}},
        [](const nlohmann::json&) { return nlohmann::json{}; }
    );

    reg->remove_tool("removable");

    auto result = reg->invoke("removable", {});
    EXPECT_FALSE(result.has_value());
}

// --- Coverage: line 29 (duplicate tool registration replaces existing) ---
TEST(ToolRegistryTest, DuplicateRegistrationOverwrites) {
    auto reg = make_tool_registry();
    reg->register_tool(
        {.name = "dup", .description = "Original", .input_schema = {}},
        [](const nlohmann::json&) { return nlohmann::json{{"v", 1}}; }
    );
    reg->register_tool(
        {.name = "dup", .description = "Replacement", .input_schema = {}},
        [](const nlohmann::json&) { return nlohmann::json{{"v", 2}}; }
    );

    auto tools = reg->list_tools();
    EXPECT_EQ(tools.size(), 1u);
    EXPECT_EQ(tools[0].description, "Replacement");

    auto result = reg->invoke("dup", {});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)["v"], 2);
}

} // namespace
} // namespace euxis::runtime
