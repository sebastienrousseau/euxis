#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "euxis/runtime/tool_manifest.hpp"

namespace euxis::runtime {
namespace {

class ToolManifestTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_tool_manifest_test";
        std::filesystem::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }

    auto write_manifest(const std::string& name, const nlohmann::json& j) -> std::string {
        auto path = tmp_dir_ / (name + ".json");
        std::ofstream f(path);
        f << j.dump(2);
        return path.string();
    }
};

TEST_F(ToolManifestTest, LoadValid) {
    nlohmann::json j = {
        {"agent_id", "agent-01"},
        {"version", "1.0"},
        {"tools", {{
            {"name", "read_file"},
            {"description", "Read a file"},
            {"input_schema", {{"type", "object"}}},
            {"source", "builtin"},
            {"required_scope", "read"},
            {"requires_approval", false},
        }}},
    };
    auto path = write_manifest("valid", j);
    auto result = load_tool_manifest(path);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->agent_id, "agent-01");
    EXPECT_EQ(result->tools.size(), 1u);
    EXPECT_EQ(result->tools[0].declaration.name, "read_file");
    EXPECT_EQ(result->tools[0].source, "builtin");
}

TEST_F(ToolManifestTest, LoadNonexistent) {
    auto result = load_tool_manifest("/nonexistent/path.json");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ToolManifestTest, MergeOverrides) {
    ToolManifest base;
    base.agent_id = "agent-01";
    base.version = "1.0";
    base.tools.push_back({
        .declaration = {.name = "tool_a", .description = "Original A", .input_schema = {}},
        .source = "builtin",
        .required_scope = {},
        .requires_approval = false,
    });
    base.tools.push_back({
        .declaration = {.name = "tool_b", .description = "Original B", .input_schema = {}},
        .source = "builtin",
        .required_scope = {},
        .requires_approval = false,
    });

    ToolManifest overlay;
    overlay.version = "2.0";
    overlay.tools.push_back({
        .declaration = {.name = "tool_a", .description = "Override A", .input_schema = {}},
        .source = "plugin",
        .required_scope = {},
        .requires_approval = false,
    });
    overlay.tools.push_back({
        .declaration = {.name = "tool_c", .description = "New C", .input_schema = {}},
        .source = "plugin",
        .required_scope = {},
        .requires_approval = false,
    });

    auto merged = merge_manifests(base, overlay);
    EXPECT_EQ(merged.version, "2.0");
    EXPECT_EQ(merged.tools.size(), 3u);

    // tool_a was overridden
    auto it_a = std::ranges::find_if(merged.tools,
        [](const ToolManifestEntry& e) { return e.declaration.name == "tool_a"; });
    ASSERT_NE(it_a, merged.tools.end());
    EXPECT_EQ(it_a->declaration.description, "Override A");
    EXPECT_EQ(it_a->source, "plugin");
}

TEST_F(ToolManifestTest, ValidateScopePresent) {
    ToolManifest manifest;
    manifest.tools.push_back({
        .declaration = {.name = "read", .description = "Read", .input_schema = {}},
        .source = "builtin",
        .required_scope = "read",
        .requires_approval = false,
    });

    auto result = validate_tool_manifest(manifest, {"read", "write"});
    EXPECT_TRUE(result.has_value());
}

TEST_F(ToolManifestTest, ValidateScopeMissing) {
    ToolManifest manifest;
    manifest.tools.push_back({
        .declaration = {.name = "deploy", .description = "Deploy", .input_schema = {}},
        .source = "plugin",
        .required_scope = "admin",
        .requires_approval = true,
    });

    auto result = validate_tool_manifest(manifest, {"read", "write"});
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("admin"), std::string::npos);
}

// --- Coverage: line 19 (malformed JSON in manifest file) ---
TEST_F(ToolManifestTest, LoadMalformedJson) {
    auto path = write_manifest("bad", nlohmann::json{});
    // Overwrite with invalid JSON
    std::ofstream f(path);
    f << "not valid json at all {{{";
    f.close();
    auto result = load_tool_manifest(path);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("parse"), std::string::npos);
}

// --- Coverage: line 58 (merge_manifests with empty overlay version) ---
TEST_F(ToolManifestTest, MergeEmptyOverlayVersionKeepsBase) {
    ToolManifest base;
    base.version = "1.0";

    ToolManifest overlay;
    overlay.version = "";  // empty overlay version

    auto merged = merge_manifests(base, overlay);
    EXPECT_EQ(merged.version, "1.0");
}

// --- Coverage: line 65 (validate_tool_manifest with empty tool name) ---
TEST_F(ToolManifestTest, ValidateEmptyToolNameFails) {
    ToolManifest manifest;
    manifest.tools.push_back({
        .declaration = {.name = "", .description = "No name", .input_schema = {}},
        .source = "builtin",
        .required_scope = {},
        .requires_approval = false,
    });

    auto result = validate_tool_manifest(manifest, {});
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("empty name"), std::string::npos);
}

} // namespace
} // namespace euxis::runtime
