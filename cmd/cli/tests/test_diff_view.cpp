#include <gtest/gtest.h>
#include "euxis/cli/tui/diff_view.hpp"

namespace euxis::cli::tui {
namespace {

const std::string_view sample_diff = R"(--- a/main.cpp
+++ b/main.cpp
@@ -1,5 +1,6 @@
 #include <iostream>

-int main() {
+int main(int argc, char** argv) {
+    if (argc > 1) std::cout << argv[1];
     std::cout << "Hello\n";
     return 0;
 })";

// --- Diff Parser ---

TEST(DiffParserTest, ParsesFileNames) {
    auto diff = parse_unified_diff(sample_diff);
    EXPECT_EQ(diff.old_file, "a/main.cpp");
    EXPECT_EQ(diff.new_file, "b/main.cpp");
}

TEST(DiffParserTest, ParsesLines) {
    auto diff = parse_unified_diff(sample_diff);
    EXPECT_GT(diff.lines.size(), 0u);
}

TEST(DiffParserTest, ParsesHunkHeader) {
    auto diff = parse_unified_diff(sample_diff);
    bool found_header = false;
    for (const auto& l : diff.lines) {
        if (l.type == DiffLine::Type::Header) { found_header = true; break; }
    }
    EXPECT_TRUE(found_header);
}

TEST(DiffParserTest, ParsesAdditions) {
    auto diff = parse_unified_diff(sample_diff);
    int add_count = 0;
    for (const auto& l : diff.lines) {
        if (l.type == DiffLine::Type::Addition) add_count++;
    }
    EXPECT_EQ(add_count, 2);
}

TEST(DiffParserTest, ParsesDeletions) {
    auto diff = parse_unified_diff(sample_diff);
    int del_count = 0;
    for (const auto& l : diff.lines) {
        if (l.type == DiffLine::Type::Deletion) del_count++;
    }
    EXPECT_EQ(del_count, 1);
}

TEST(DiffParserTest, ParsesContext) {
    auto diff = parse_unified_diff(sample_diff);
    int ctx_count = 0;
    for (const auto& l : diff.lines) {
        if (l.type == DiffLine::Type::Context) ctx_count++;
    }
    EXPECT_GT(ctx_count, 0);
}

TEST(DiffParserTest, EmptyDiff) {
    auto diff = parse_unified_diff("");
    EXPECT_TRUE(diff.lines.empty());
}

TEST(DiffParserTest, LineNumbers) {
    auto diff = parse_unified_diff(sample_diff);
    for (const auto& l : diff.lines) {
        if (l.type == DiffLine::Type::Addition) {
            EXPECT_GE(l.new_line, 0);
            EXPECT_EQ(l.old_line, -1);
        }
        if (l.type == DiffLine::Type::Deletion) {
            EXPECT_GE(l.old_line, 0);
            EXPECT_EQ(l.new_line, -1);
        }
    }
}

// --- DiffView Widget ---

TEST(DiffViewTest, ConstructDestruct) {
    DiffView dv;
    EXPECT_TRUE(dv.focusable());
    EXPECT_EQ(dv.scroll_offset(), 0);
    EXPECT_EQ(dv.line_count(), 0);
}

TEST(DiffViewTest, SetDiffText) {
    DiffView dv;
    dv.set_diff_text(sample_diff);
    EXPECT_GT(dv.line_count(), 0);
}

TEST(DiffViewTest, SetDiff) {
    DiffView dv;
    auto diff = parse_unified_diff(sample_diff);
    int expected = static_cast<int>(diff.lines.size());
    dv.set_diff(std::move(diff));
    EXPECT_EQ(dv.line_count(), expected);
}

TEST(DiffViewTest, ScrollDown) {
    DiffView dv;
    dv.set_diff_text(sample_diff);

    Event down{EventType::Key, 1002};
    dv.handle_event(down);
    EXPECT_EQ(dv.scroll_offset(), 1);
}

TEST(DiffViewTest, ScrollUp) {
    DiffView dv;
    dv.set_diff_text(sample_diff);

    Event down{EventType::Key, 1002};
    dv.handle_event(down);
    int after_down = dv.scroll_offset();
    EXPECT_EQ(after_down, 1);

    Event up{EventType::Key, 1001};
    dv.handle_event(up);
    EXPECT_EQ(dv.scroll_offset(), 0);
}

TEST(DiffViewTest, ScrollUpAtTopStays) {
    DiffView dv;
    dv.set_diff_text(sample_diff);

    Event up{EventType::Key, 1001};
    dv.handle_event(up);
    EXPECT_EQ(dv.scroll_offset(), 0);
}

TEST(DiffViewTest, HomeKey) {
    DiffView dv;
    dv.set_diff_text(sample_diff);

    Event down{EventType::Key, 1002};
    dv.handle_event(down);
    dv.handle_event(down);

    Event home{EventType::Key, 1010};
    dv.handle_event(home);
    EXPECT_EQ(dv.scroll_offset(), 0);
}

TEST(DiffViewTest, PageDown) {
    DiffView dv;
    dv.set_diff_text(sample_diff);

    Event pgdn{EventType::Key, 1013};
    dv.handle_event(pgdn);
    EXPECT_GE(dv.scroll_offset(), 0);
}

TEST(DiffViewTest, VimKeys) {
    DiffView dv;
    dv.set_diff_text(sample_diff);

    Event j{EventType::Key, 'j'};
    dv.handle_event(j);
    EXPECT_EQ(dv.scroll_offset(), 1);

    Event k{EventType::Key, 'k'};
    dv.handle_event(k);
    EXPECT_EQ(dv.scroll_offset(), 0);
}

TEST(DiffViewTest, RenderDoesNotCrash) {
    DiffView dv;
    dv.set_diff_text(sample_diff);

    terminal::TerminalScreen screen;
    dv.render(screen, {0, 0, 80, 20});
}

TEST(DiffViewTest, RenderEmptyDiff) {
    DiffView dv;
    terminal::TerminalScreen screen;
    dv.render(screen, {0, 0, 80, 20});
}

TEST(DiffViewTest, RenderWithColorSystem) {
    DiffView dv;
    dv.set_diff_text(sample_diff);

    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});
    dv.set_color_system(&cs);

    terminal::TerminalScreen screen;
    dv.render(screen, {0, 0, 80, 20});
}

TEST(DiffViewTest, PreferredSize) {
    DiffView dv;
    dv.set_diff_text(sample_diff);
    auto sz = dv.preferred_size();
    EXPECT_EQ(sz.width, 80);
    EXPECT_GT(sz.height, 0);
}

TEST(DiffViewTest, AccessibleName) {
    DiffView dv;
    EXPECT_FALSE(dv.accessible_name.empty());
}

} // namespace
} // namespace euxis::cli::tui
