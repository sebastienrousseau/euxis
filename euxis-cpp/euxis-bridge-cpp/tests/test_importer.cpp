#include <gtest/gtest.h>
#include <euxis/bridge/importer.hpp>

#include <filesystem>
#include <fstream>

namespace euxis::bridge {

class ImporterTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_test_importer";
        std::filesystem::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }

    void create_skill(const std::string& name,
                      const std::string& runtime = "node",
                      bool with_openclaw = false) {
        auto dir = tmp_dir_ / name;
        std::filesystem::create_directories(dir);

        std::ofstream skill_md(dir / "SKILL.md");
        skill_md << "---\n"
                 << "name: " << name << "\n"
                 << "runtime: " << runtime << "\n"
                 << "entrypoint: index.js\n"
                 << "description: Test skill " << name << "\n"
                 << "tags: test, demo\n"
                 << "---\n"
                 << "# " << name << "\n";
        skill_md.close();

        // Create entrypoint
        std::ofstream ep(dir / "index.js");
        ep << "console.log('hello');\n";
        ep.close();

        if (with_openclaw) {
            std::ofstream oc(dir / "openclaw.json");
            oc << R"({"description": "OpenClaw description", "metadata": {"source": "openclaw"}})";
            oc.close();
        }
    }
};

TEST_F(ImporterTest, ImportSingleSkill) {
    create_skill("test-skill");
    ClawHubImporter importer(tmp_dir_);

    auto result = importer.import_skill(tmp_dir_ / "test-skill");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "test-skill");
    EXPECT_EQ(result->runtime, "node");
    EXPECT_EQ(result->tags.size(), 2);
}

TEST_F(ImporterTest, ImportWithOpenclaw) {
    create_skill("oc-skill", "node", true);
    ClawHubImporter importer(tmp_dir_);

    auto result = importer.import_skill(tmp_dir_ / "oc-skill");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->metadata["source"], "clawhub");
}

TEST_F(ImporterTest, ImportAll) {
    create_skill("skill-a");
    create_skill("skill-b", "python");
    create_skill("skill-c");

    ClawHubImporter importer(tmp_dir_);
    auto skills = importer.import_all();

    EXPECT_EQ(skills.size(), 3);
}

TEST_F(ImporterTest, ImportNonexistent) {
    ClawHubImporter importer(tmp_dir_ / "nonexistent");
    auto skills = importer.import_all();

    EXPECT_TRUE(skills.empty());
    EXPECT_FALSE(importer.errors().empty());
}

TEST_F(ImporterTest, ImportInvalidSkill) {
    auto dir = tmp_dir_ / "bad-skill";
    std::filesystem::create_directories(dir);
    // No SKILL.md — import_all should skip it

    ClawHubImporter importer(tmp_dir_);
    auto skills = importer.import_all();
    EXPECT_TRUE(skills.empty());
}

TEST_F(ImporterTest, MetadataSourceTag) {
    create_skill("meta-skill");
    ClawHubImporter importer(tmp_dir_);

    auto result = importer.import_skill(tmp_dir_ / "meta-skill");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->metadata["source"], "clawhub");
}

}  // namespace euxis::bridge
