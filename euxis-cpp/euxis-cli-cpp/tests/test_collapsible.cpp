#include <gtest/gtest.h>
#include "euxis/cli/tui/collapsible.hpp"

namespace euxis::cli::tui {
namespace {

TEST(CollapsibleTest, ConstructThinking) {
    Collapsible c(Collapsible::SectionType::Thinking, "Analyzing code");
    EXPECT_FALSE(c.expanded());
    EXPECT_FALSE(c.active());
    EXPECT_TRUE(c.focusable());
}

TEST(CollapsibleTest, ConstructToolUse) {
    Collapsible c(Collapsible::SectionType::ToolUse, "read_file main.cpp", "file contents here");
    EXPECT_FALSE(c.expanded());
}

TEST(CollapsibleTest, ConstructContext) {
    Collapsible c(Collapsible::SectionType::Context, "3 files loaded");
    EXPECT_FALSE(c.expanded());
}

TEST(CollapsibleTest, Toggle) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test");
    EXPECT_FALSE(c.expanded());
    c.toggle();
    EXPECT_TRUE(c.expanded());
    c.toggle();
    EXPECT_FALSE(c.expanded());
}

TEST(CollapsibleTest, SetExpanded) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test");
    c.set_expanded(true);
    EXPECT_TRUE(c.expanded());
    c.set_expanded(false);
    EXPECT_FALSE(c.expanded());
}

TEST(CollapsibleTest, SetActive) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test");
    c.set_active(true);
    EXPECT_TRUE(c.active());
}

TEST(CollapsibleTest, SetElapsed) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test");
    c.set_elapsed(std::chrono::seconds(12));
    // Just verify no crash
}

TEST(CollapsibleTest, SetContent) {
    Collapsible c(Collapsible::SectionType::ToolUse, "read_file");
    c.set_content("line 1\nline 2\nline 3");
    // Content should influence preferred size when expanded
    c.set_expanded(true);
    auto sz = c.preferred_size();
    EXPECT_GE(sz.height, 4); // 1 header + 3 content lines
}

TEST(CollapsibleTest, PreferredSizeCollapsed) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test", "some\ncontent");
    c.set_expanded(false);
    auto sz = c.preferred_size();
    EXPECT_EQ(sz.height, 1); // Just the header
}

TEST(CollapsibleTest, PreferredSizeExpanded) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test", "line1\nline2");
    c.set_expanded(true);
    auto sz = c.preferred_size();
    EXPECT_EQ(sz.height, 3); // Header + 2 content lines
}

TEST(CollapsibleTest, HandleEventToggle) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test");
    EXPECT_FALSE(c.expanded());

    Event enter{EventType::Key, '\r'};
    EXPECT_TRUE(c.handle_event(enter));
    EXPECT_TRUE(c.expanded());

    Event space{EventType::Key, ' '};
    EXPECT_TRUE(c.handle_event(space));
    EXPECT_FALSE(c.expanded());
}

TEST(CollapsibleTest, HandleEventIgnoresOtherKeys) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test");
    Event arrow{EventType::Key, 1001};
    EXPECT_FALSE(c.handle_event(arrow));
}

TEST(CollapsibleTest, HandleEventIgnoresNonKeyEvents) {
    Collapsible c(Collapsible::SectionType::Thinking, "Test");
    Event resize{EventType::Resize, 0, 80, 24};
    EXPECT_FALSE(c.handle_event(resize));
}

TEST(CollapsibleTest, RenderCollapsedDoesNotCrash) {
    Collapsible c(Collapsible::SectionType::Thinking, "Analyzing", "detailed output");
    c.set_elapsed(std::chrono::seconds(5));

    terminal::TerminalScreen screen;
    c.render(screen, {0, 0, 80, 5});
}

TEST(CollapsibleTest, RenderExpandedDoesNotCrash) {
    Collapsible c(Collapsible::SectionType::ToolUse, "read_file", "file\ncontents\nhere");
    c.set_expanded(true);

    terminal::TerminalScreen screen;
    c.render(screen, {0, 0, 80, 10});
}

TEST(CollapsibleTest, RenderWithColorSystem) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});

    Collapsible c(Collapsible::SectionType::Context, "3 files", "a.cpp\nb.cpp\nc.cpp");
    c.set_color_system(&cs);
    c.set_expanded(true);

    terminal::TerminalScreen screen;
    c.render(screen, {0, 0, 80, 10});
}

TEST(CollapsibleTest, EmptyContent) {
    Collapsible c(Collapsible::SectionType::Thinking, "Empty");
    c.set_expanded(true);
    auto sz = c.preferred_size();
    EXPECT_EQ(sz.height, 1); // Just header, no content
}

TEST(CollapsibleTest, AccessibleName) {
    Collapsible c(Collapsible::SectionType::ToolUse, "read_file");
    EXPECT_FALSE(c.accessible_name.empty());
    EXPECT_NE(c.accessible_name.find("read_file"), std::string::npos);
}

} // namespace
} // namespace euxis::cli::tui
