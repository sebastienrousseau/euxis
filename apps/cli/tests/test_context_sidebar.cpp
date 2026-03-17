#include <gtest/gtest.h>
#include "euxis/cli/tui/context_sidebar.hpp"

namespace euxis::cli::tui {
namespace {

TEST(ContextSidebarTest, ConstructDefault) {
    ContextSidebar cs;
    EXPECT_FALSE(cs.accessible_name.empty());
    EXPECT_FALSE(cs.accessible_description.empty());
}

TEST(ContextSidebarTest, PreferredSize) {
    ContextSidebar cs;
    auto sz = cs.preferred_size();
    EXPECT_EQ(sz.width, 25);
    EXPECT_EQ(sz.height, 20);
}

TEST(ContextSidebarTest, SetGitBranch) {
    ContextSidebar cs;
    cs.set_git_branch("main");
    // Verify no crash on render
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetModifiedCount) {
    ContextSidebar cs;
    cs.set_git_branch("feature/test");
    cs.set_modified_count(5);
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetModifiedCountZero) {
    ContextSidebar cs;
    cs.set_git_branch("main");
    cs.set_modified_count(0);
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetModel) {
    ContextSidebar cs;
    cs.set_model("claude-sonnet-4-6");
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetModelLongName) {
    ContextSidebar cs;
    cs.set_model("some-very-long-model-name-that-exceeds-column-width-limit");
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 25, 20});
}

TEST(ContextSidebarTest, SetProvider) {
    ContextSidebar cs;
    cs.set_provider("anthropic");
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetTokenCountSmall) {
    ContextSidebar cs;
    cs.set_token_count(500);
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetTokenCountLarge) {
    ContextSidebar cs;
    cs.set_token_count(24000);
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetCost) {
    ContextSidebar cs;
    cs.set_cost(0.0234);
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetCostZero) {
    ContextSidebar cs;
    cs.set_cost(0.0);
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetAgent) {
    ContextSidebar cs;
    cs.set_agent("code-agent");
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetMcpTools) {
    ContextSidebar cs;
    cs.set_mcp_tools({"read_file", "write_file", "search"});
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, SetMcpToolsEmpty) {
    ContextSidebar cs;
    cs.set_mcp_tools({});
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, RenderEmptyDoesNotCrash) {
    ContextSidebar cs;
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, RenderEmptyAreaDoesNotCrash) {
    ContextSidebar cs;
    cs.set_git_branch("main");
    cs.set_model("claude-sonnet-4-6");
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 0, 0});
}

TEST(ContextSidebarTest, RenderTooNarrow) {
    ContextSidebar cs;
    cs.set_git_branch("main");
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 5, 20}); // Width < 10 threshold
}

TEST(ContextSidebarTest, RenderFullPopulated) {
    ContextSidebar cs;
    cs.set_git_branch("feature/tui-gold");
    cs.set_modified_count(3);
    cs.set_model("claude-sonnet-4-6");
    cs.set_provider("anthropic");
    cs.set_token_count(2400);
    cs.set_cost(0.0234);
    cs.set_agent("code-agent");
    cs.set_mcp_tools({"read_file", "write_file"});

    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 25});
}

TEST(ContextSidebarTest, RenderWithColorSystem) {
    ContextSidebar cs;
    cs.set_git_branch("main");
    cs.set_model("claude-sonnet-4-6");
    cs.set_token_count(1500);
    cs.set_agent("architect");

    ColorSystem colors;
    colors.load_palette("tokyo-dark");
    colors.set_background({0, 0, 0});
    cs.set_color_system(&colors);

    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 20});
}

TEST(ContextSidebarTest, RenderTruncatedHeight) {
    ContextSidebar cs;
    cs.set_git_branch("main");
    cs.set_modified_count(5);
    cs.set_model("claude-sonnet-4-6");
    cs.set_provider("anthropic");
    cs.set_token_count(2400);
    cs.set_cost(0.02);
    cs.set_agent("code-agent");
    cs.set_mcp_tools({"tool1", "tool2", "tool3", "tool4", "tool5"});

    // Very small height -- should not crash
    terminal::TerminalScreen screen;
    cs.render(screen, {0, 0, 30, 5});
}

TEST(ContextSidebarTest, AccessibleFields) {
    ContextSidebar cs;
    EXPECT_EQ(cs.accessible_name, "Context Sidebar");
    EXPECT_NE(cs.accessible_description.find("git"), std::string::npos);
}

} // namespace
} // namespace euxis::cli::tui
