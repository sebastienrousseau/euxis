#include <gtest/gtest.h>
#include "euxis/cli/tui/text_utils.hpp"

using namespace euxis::cli::tui;

TEST(TextUtilsTest, WrapSimpleText) {
    std::string text = "This is a long sentence that should be wrapped.";
    auto lines = wrap_text(text, 10);
    
    ASSERT_GT(lines.size(), 1);
    for (const auto& line : lines) {
        EXPECT_LE(line.size(), 10);
    }
}

TEST(TextUtilsTest, WrapTextWithNewlines) {
    std::string text = "Line 1\nLine 2 is longer and should wrap.";
    auto lines = wrap_text(text, 10);
    
    EXPECT_EQ(lines[0], "Line 1");
    // Line 2 is (9 chars)
    // longer and (10 chars)
    // should (6 chars)
    // wrap. (5 chars)
    EXPECT_EQ(lines[1], "Line 2 is");
    EXPECT_EQ(lines[2], "longer and");
}

TEST(TextUtilsTest, WrapLongWord) {
    std::string text = "Supercalifragilisticexpialidocious";
    auto lines = wrap_text(text, 10);
    
    EXPECT_GT(lines.size(), 1);
    for (const auto& line : lines) {
        EXPECT_LE(line.size(), 10);
    }
}
