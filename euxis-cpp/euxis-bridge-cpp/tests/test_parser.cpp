#include <gtest/gtest.h>
#include <euxis/bridge/parser.hpp>

#include <filesystem>
#include <fstream>

namespace euxis::bridge {

TEST(ParserTest, ParseFrontmatter) {
    std::string content = R"(---
name: test-skill
runtime: node
entrypoint: index.js
description: A test skill
---
# Test Skill

This is the body.
)";

    auto fm = parse_frontmatter(content);
    EXPECT_EQ(fm.get("name"), "test-skill");
    EXPECT_EQ(fm.get("runtime"), "node");
    EXPECT_EQ(fm.get("entrypoint"), "index.js");
    EXPECT_EQ(fm.get("description"), "A test skill");
    EXPECT_TRUE(fm.body.find("Test Skill") != std::string::npos);
}

TEST(ParserTest, MissingField) {
    std::string content = R"(---
name: test
---
Body text.
)";

    auto fm = parse_frontmatter(content);
    EXPECT_EQ(fm.get("name"), "test");
    EXPECT_EQ(fm.get("runtime"), std::nullopt);
}

TEST(ParserTest, NoFrontmatter) {
    std::string content = "Just plain text\nNo frontmatter here.\n";
    auto fm = parse_frontmatter(content);
    EXPECT_TRUE(fm.fields.empty());
    EXPECT_TRUE(fm.body.find("Just plain text") != std::string::npos);
}

TEST(ParserTest, EmptyContent) {
    auto fm = parse_frontmatter("");
    EXPECT_TRUE(fm.fields.empty());
    EXPECT_TRUE(fm.body.empty());
}

TEST(ParserTest, ParseSkillFile) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_test_parser";
    std::filesystem::create_directories(tmp);
    auto path = tmp / "SKILL.md";

    std::ofstream f(path);
    f << "---\nname: file-skill\nruntime: python\n---\nBody.\n";
    f.close();

    auto fm = parse_skill_file(path);
    ASSERT_TRUE(fm.has_value());
    EXPECT_EQ(fm->get("name"), "file-skill");
    EXPECT_EQ(fm->get("runtime"), "python");

    std::filesystem::remove_all(tmp);
}

TEST(ParserTest, ParseSkillFileNotFound) {
    auto result = parse_skill_file("/nonexistent/SKILL.md");
    EXPECT_FALSE(result.has_value());
}

TEST(ParserTest, WhitespaceHandling) {
    std::string content = "---\n  name  :  spaced  \nruntime:node\n---\n";
    auto fm = parse_frontmatter(content);
    EXPECT_EQ(fm.get("name"), "spaced");
    EXPECT_EQ(fm.get("runtime"), "node");
}

}  // namespace euxis::bridge
