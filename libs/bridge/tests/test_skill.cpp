#include <gtest/gtest.h>
#include <euxis/bridge/skill.hpp>

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::bridge {

TEST(BridgedSkillTest, ToJson) {
    BridgedSkill skill;
    skill.name = "test-skill";
    skill.slug = "test_skill";
    skill.source_dir = "/tmp/skills/test";
    skill.description = "A test skill";
    skill.runtime = "node";
    skill.entrypoint = "/tmp/skills/test/index.js";
    skill.tags = {"test", "demo"};
    skill.metadata = {{"version", "1.0"}};

    auto j = skill.to_json();
    EXPECT_EQ(j["name"], "test-skill");
    EXPECT_EQ(j["slug"], "test_skill");
    EXPECT_EQ(j["runtime"], "node");
    EXPECT_EQ(j["tags"].size(), 2);
    EXPECT_EQ(j["tags"][0], "test");
}

TEST(BridgedSkillTest, FromJson) {
    nlohmann::json j;
    j["name"] = "imported";
    j["slug"] = "imported";
    j["description"] = "Imported skill";
    j["runtime"] = "python";
    j["entrypoint"] = "main.py";
    j["tags"] = {"ai", "ml"};

    auto skill = BridgedSkill::from_json(j);
    EXPECT_EQ(skill.name, "imported");
    EXPECT_EQ(skill.runtime, "python");
    EXPECT_EQ(skill.tags.size(), 2);
}

TEST(BridgedSkillTest, RoundTrip) {
    BridgedSkill original;
    original.name = "roundtrip";
    original.slug = "roundtrip";
    original.source_dir = "/src";
    original.description = "Roundtrip test";
    original.runtime = "node";
    original.entrypoint = "index.js";
    original.tags = {"a", "b", "c"};
    original.metadata = {{"key", "value"}};

    auto j = original.to_json();
    auto restored = BridgedSkill::from_json(j);

    EXPECT_EQ(original.name, restored.name);
    EXPECT_EQ(original.slug, restored.slug);
    EXPECT_EQ(original.runtime, restored.runtime);
    EXPECT_EQ(original.tags, restored.tags);
}

TEST(BridgedSkillTest, OptionalSignaturePath) {
    BridgedSkill skill;
    skill.name = "signed";
    skill.signature_path = "/tmp/sig.sig";

    auto j = skill.to_json();
    EXPECT_TRUE(j.contains("signature_path"));

    BridgedSkill unsigned_skill;
    unsigned_skill.name = "unsigned";

    auto j2 = unsigned_skill.to_json();
    EXPECT_FALSE(j2.contains("signature_path"));
}

TEST(BridgedSkillTest, OptionalOutputSchema) {
    BridgedSkill skill;
    skill.name = "schema";
    skill.output_schema = nlohmann::json{{"type", "object"}};

    auto j = skill.to_json();
    EXPECT_TRUE(j.contains("output_schema"));
    EXPECT_EQ(j["output_schema"]["type"], "object");
}

// --- Coverage: line 22 (to_json without optional fields) ---
TEST(BridgedSkillTest, ToJsonMinimalSkill) {
    BridgedSkill skill;
    skill.name = "minimal";
    skill.slug = "minimal";
    skill.source_dir = "/tmp";
    skill.runtime = "node";
    skill.entrypoint = "index.js";
    // No signature_path, no output_schema

    auto j = skill.to_json();
    EXPECT_FALSE(j.contains("signature_path"));
    EXPECT_FALSE(j.contains("output_schema"));
    EXPECT_EQ(j["name"], "minimal");
}

// --- Coverage: line 41 (from_json with signature_path) ---
TEST(BridgedSkillTest, FromJsonWithSignaturePath) {
    nlohmann::json j;
    j["name"] = "signed-skill";
    j["slug"] = "signed_skill";
    j["signature_path"] = "/tmp/sig.sig";
    j["output_schema"] = {{"type", "string"}};

    auto skill = BridgedSkill::from_json(j);
    ASSERT_TRUE(skill.signature_path.has_value());
    EXPECT_EQ(skill.signature_path->string(), "/tmp/sig.sig");
    ASSERT_TRUE(skill.output_schema.has_value());
    EXPECT_EQ((*skill.output_schema)["type"], "string");
}

// --- Coverage: line 44 (from_json without source_dir and entrypoint) ---
TEST(BridgedSkillTest, FromJsonMissingOptionalFields) {
    nlohmann::json j;
    j["name"] = "bare";
    // No source_dir, no entrypoint, no tags, no signature_path, no output_schema

    auto skill = BridgedSkill::from_json(j);
    EXPECT_EQ(skill.name, "bare");
    EXPECT_EQ(skill.runtime, "node");  // default
    EXPECT_TRUE(skill.tags.empty());
    EXPECT_FALSE(skill.signature_path.has_value());
    EXPECT_FALSE(skill.output_schema.has_value());
}

// --- Coverage: line 47 (from_json with empty metadata) ---
TEST(BridgedSkillTest, FromJsonDefaultMetadata) {
    nlohmann::json j;
    j["name"] = "meta-default";
    // No metadata key in JSON

    auto skill = BridgedSkill::from_json(j);
    EXPECT_TRUE(skill.metadata.is_object());
    EXPECT_TRUE(skill.metadata.empty());
}

// --- Coverage: line 22 (to_json with both optional fields present) ---
TEST(BridgedSkillTest, ToJsonWithAllOptionals) {
    BridgedSkill skill;
    skill.name = "full-skill";
    skill.slug = "full_skill";
    skill.source_dir = "/tmp/full";
    skill.description = "Full skill with all fields";
    skill.runtime = "python";
    skill.entrypoint = "main.py";
    skill.tags = {"ai", "ml", "prod"};
    skill.metadata = {{"author", "test"}, {"version", "3.0"}};
    skill.signature_path = "/tmp/full/main.py.sig";
    skill.output_schema = nlohmann::json{{"type", "array"}, {"items", {{"type", "string"}}}};

    auto j = skill.to_json();
    EXPECT_TRUE(j.contains("signature_path"));
    EXPECT_TRUE(j.contains("output_schema"));
    EXPECT_EQ(j["output_schema"]["type"], "array");
    EXPECT_EQ(j["tags"].size(), 3u);
}

// --- Coverage: line 47 (from_json complete roundtrip with all fields) ---
TEST(BridgedSkillTest, FromJsonCompleteRoundtrip) {
    BridgedSkill original;
    original.name = "complete";
    original.slug = "complete_skill";
    original.source_dir = "/src/complete";
    original.description = "Complete roundtrip";
    original.runtime = "python";
    original.entrypoint = "main.py";
    original.tags = {"x", "y"};
    original.metadata = {{"key1", "val1"}};
    original.signature_path = "/sig.sig";
    original.output_schema = nlohmann::json{{"type", "number"}};

    auto j = original.to_json();
    auto restored = BridgedSkill::from_json(j);

    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.slug, original.slug);
    EXPECT_EQ(restored.description, original.description);
    EXPECT_EQ(restored.runtime, original.runtime);
    EXPECT_EQ(restored.tags, original.tags);
    ASSERT_TRUE(restored.signature_path.has_value());
    EXPECT_EQ(restored.signature_path->string(), "/sig.sig");
    ASSERT_TRUE(restored.output_schema.has_value());
    EXPECT_EQ((*restored.output_schema)["type"], "number");
}

}  // namespace euxis::bridge

// NOLINTEND(bugprone-unchecked-optional-access)
