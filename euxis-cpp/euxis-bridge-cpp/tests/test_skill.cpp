#include <gtest/gtest.h>
#include <euxis/bridge/skill.hpp>

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

}  // namespace euxis::bridge
