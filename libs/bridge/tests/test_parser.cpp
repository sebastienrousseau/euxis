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

TEST(ParserTest, FrontmatterLineWithoutColon) {
    // Parsing error path: line in frontmatter with no colon is ignored (line 52)
    std::string content = "---\nname: valid\nthis-has-no-colon\nruntime: python\n---\nBody.\n";
    auto fm = parse_frontmatter(content);
    EXPECT_EQ(fm.get("name"), "valid");
    EXPECT_EQ(fm.get("runtime"), "python");
    // The line without a colon should not produce any field
    EXPECT_EQ(fm.fields.size(), 2u);
}

TEST(ParserTest, CarriageReturnTrimming) {
    // Parsing path: lines with trailing \r are trimmed (line 27)
    std::string content = "---\r\nname: crlf-test\r\nruntime: node\r\n---\r\nBody text.\r\n";
    auto fm = parse_frontmatter(content);
    EXPECT_EQ(fm.get("name"), "crlf-test");
    EXPECT_EQ(fm.get("runtime"), "node");
}

TEST(ParserTest, WhitespaceOnlyValue) {
    // Parsing edge case: a key with whitespace-only value (line 52: s.clear())
    std::string content = "---\nempty_val:   \nname: test\n---\nBody.\n";
    auto fm = parse_frontmatter(content);
    EXPECT_EQ(fm.get("name"), "test");
    // empty_val should have an empty string value after trimming
    auto val = fm.get("empty_val");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(val->empty());
}

}  // namespace euxis::bridge
