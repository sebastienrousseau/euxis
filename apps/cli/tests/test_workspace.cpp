#include <gtest/gtest.h>
#include "euxis/cli/tui/workspace.hpp"
#include "euxis/cli/tui/context_sidebar.hpp"

namespace euxis::cli::tui {
namespace {

// ---------------------------------------------------------------------------
// FileTree
// ---------------------------------------------------------------------------

TEST(FileTreeTest, ConstructEmpty) {
    FileTree ft;
    auto sz = ft.preferred_size();
    EXPECT_EQ(sz.width, 20);
    EXPECT_EQ(sz.height, 0);
    EXPECT_TRUE(ft.focusable());
}

TEST(FileTreeTest, SetEntries) {
    FileTree ft;
    ft.set_entries({
        {"src", true, false, 0},
        {"main.cpp", false, false, 1},
        {"lib", true, false, 0},
    });
    auto sz = ft.preferred_size();
    EXPECT_EQ(sz.height, 3);
}

TEST(FileTreeTest, NavigateDown) {
    FileTree ft;
    ft.set_entries({
        {"a.cpp", false, false, 0},
        {"b.cpp", false, false, 0},
        {"c.cpp", false, false, 0},
    });

    Event down{EventType::Key, 1002};
    EXPECT_TRUE(ft.handle_event(down));
}

TEST(FileTreeTest, NavigateUp) {
    FileTree ft;
    ft.set_entries({
        {"a.cpp", false, false, 0},
        {"b.cpp", false, false, 0},
    });

    Event down{EventType::Key, 1002};
    ft.handle_event(down);

    Event up{EventType::Key, 1001};
    EXPECT_TRUE(ft.handle_event(up));
}

TEST(FileTreeTest, NavigateUpAtTopStays) {
    FileTree ft;
    ft.set_entries({{"a.cpp", false, false, 0}});

    Event up{EventType::Key, 1001};
    EXPECT_TRUE(ft.handle_event(up));
}

TEST(FileTreeTest, NavigateDownAtBottomStays) {
    FileTree ft;
    ft.set_entries({{"a.cpp", false, false, 0}});

    Event down{EventType::Key, 1002};
    ft.handle_event(down);
    // Should not crash or go out of bounds
    ft.handle_event(down);
}

TEST(FileTreeTest, ToggleDirectory) {
    FileTree ft;
    ft.set_entries({
        {"src", true, false, 0},
        {"main.cpp", false, false, 1},
    });

    Event enter{EventType::Key, '\r'};
    EXPECT_TRUE(ft.handle_event(enter));
}

TEST(FileTreeTest, IgnoresUnhandledKeys) {
    FileTree ft;
    ft.set_entries({{"a.cpp", false, false, 0}});

    Event tab{EventType::Key, '\t'};
    EXPECT_FALSE(ft.handle_event(tab));
}

TEST(FileTreeTest, IgnoresNonKeyEvents) {
    FileTree ft;
    ft.set_entries({{"a.cpp", false, false, 0}});

    Event resize{EventType::Resize, 0, 80, 24};
    EXPECT_FALSE(ft.handle_event(resize));
}

TEST(FileTreeTest, RenderDoesNotCrash) {
    FileTree ft;
    ft.set_entries({
        {"src", true, true, 0},
        {"cmd", true, false, 1},
        {"tui", true, false, 1},
        {"main.cpp", false, false, 1},
    });

    terminal::TerminalScreen screen;
    ft.render(screen, {0, 0, 25, 10});
}

TEST(FileTreeTest, RenderEmptyDoesNotCrash) {
    FileTree ft;
    terminal::TerminalScreen screen;
    ft.render(screen, {0, 0, 25, 10});
}

TEST(FileTreeTest, RenderWithColorSystem) {
    FileTree ft;
    ft.set_entries({
        {"src", true, true, 0},
        {"main.cpp", false, false, 1},
    });

    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});
    ft.set_color_system(&cs);

    terminal::TerminalScreen screen;
    ft.render(screen, {0, 0, 25, 10});
}

TEST(FileTreeTest, RenderWithEmptyArea) {
    FileTree ft;
    ft.set_entries({{"a.cpp", false, false, 0}});

    terminal::TerminalScreen screen;
    ft.render(screen, {0, 0, 0, 0});
}

TEST(FileTreeTest, RenderTruncatesLongNames) {
    FileTree ft;
    ft.set_entries({
        {"very_long_filename_that_exceeds_column_width.cpp", false, false, 2},
    });

    terminal::TerminalScreen screen;
    ft.render(screen, {0, 0, 20, 5});
}

// ---------------------------------------------------------------------------
// AgentList
// ---------------------------------------------------------------------------

TEST(AgentListTest, ConstructEmpty) {
    AgentList al;
    auto sz = al.preferred_size();
    EXPECT_EQ(sz.width, 20);
    EXPECT_EQ(sz.height, 1); // just the title
}

TEST(AgentListTest, SetAgents) {
    AgentList al;
    al.set_agents({
        {"code-agent", true},
        {"reviewer", false},
        {"architect", false},
    });
    auto sz = al.preferred_size();
    EXPECT_EQ(sz.height, 4); // 3 agents + 1 title
}

TEST(AgentListTest, RenderDoesNotCrash) {
    AgentList al;
    al.set_agents({
        {"code-agent", true},
        {"reviewer", false},
    });

    terminal::TerminalScreen screen;
    al.render(screen, {0, 0, 25, 10});
}

TEST(AgentListTest, RenderEmptyDoesNotCrash) {
    AgentList al;
    terminal::TerminalScreen screen;
    al.render(screen, {0, 0, 25, 10});
}

TEST(AgentListTest, RenderWithEmptyArea) {
    AgentList al;
    al.set_agents({{"code", true}});

    terminal::TerminalScreen screen;
    al.render(screen, {0, 0, 0, 0});
}

TEST(AgentListTest, RenderWithColorSystem) {
    AgentList al;
    al.set_agents({
        {"code-agent", true},
        {"reviewer", false},
    });

    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});
    al.set_color_system(&cs);

    terminal::TerminalScreen screen;
    al.render(screen, {0, 0, 25, 10});
}

TEST(AgentListTest, TruncatesLongNames) {
    AgentList al;
    al.set_agents({
        {"very-long-agent-name-that-exceeds-width", true},
    });

    terminal::TerminalScreen screen;
    al.render(screen, {0, 0, 15, 5});
}

// ---------------------------------------------------------------------------
// LayoutMode
// ---------------------------------------------------------------------------

TEST(LayoutModeTest, SinglePaneNarrow) {
    EXPECT_EQ(layout_mode_for_width(60), LayoutMode::SinglePane);
    EXPECT_EQ(layout_mode_for_width(79), LayoutMode::SinglePane);
}

TEST(LayoutModeTest, DualPaneMedium) {
    EXPECT_EQ(layout_mode_for_width(80), LayoutMode::DualPane);
    EXPECT_EQ(layout_mode_for_width(100), LayoutMode::DualPane);
    EXPECT_EQ(layout_mode_for_width(120), LayoutMode::DualPane);
}

TEST(LayoutModeTest, TriplePaneWide) {
    EXPECT_EQ(layout_mode_for_width(121), LayoutMode::TriplePane);
    EXPECT_EQ(layout_mode_for_width(200), LayoutMode::TriplePane);
}

// ---------------------------------------------------------------------------
// Workspace
// ---------------------------------------------------------------------------

TEST(WorkspaceTest, ConstructDefault) {
    Workspace ws;
    EXPECT_EQ(ws.sidebar_visible(), true);
    EXPECT_FALSE(ws.accessible_name.empty());
}

TEST(WorkspaceTest, PreferredSize) {
    Workspace ws;
    auto sz = ws.preferred_size();
    EXPECT_EQ(sz.width, 120);
    EXPECT_EQ(sz.height, 40);
}

TEST(WorkspaceTest, ToggleSidebar) {
    Workspace ws;
    EXPECT_TRUE(ws.sidebar_visible());
    ws.toggle_sidebar();
    EXPECT_FALSE(ws.sidebar_visible());
    ws.toggle_sidebar();
    EXPECT_TRUE(ws.sidebar_visible());
}

TEST(WorkspaceTest, RenderSinglePaneDoesNotCrash) {
    Workspace ws;
    terminal::TerminalScreen screen;
    // Narrow width triggers SinglePane
    ws.render(screen, {0, 0, 60, 24});
    EXPECT_EQ(ws.current_mode(), LayoutMode::SinglePane);
}

TEST(WorkspaceTest, RenderDualPaneDoesNotCrash) {
    Workspace ws;
    auto sidebar = std::make_shared<ContextSidebar>();
    ws.set_sidebar(sidebar);

    terminal::TerminalScreen screen;
    screen.resize(100, 24);
    ws.render(screen, {0, 0, 100, 24});
    EXPECT_EQ(ws.current_mode(), LayoutMode::DualPane);
}

TEST(WorkspaceTest, RenderTriplePaneDoesNotCrash) {
    Workspace ws;
    auto sidebar = std::make_shared<ContextSidebar>();
    auto file_tree = std::make_shared<FileTree>();
    file_tree->set_entries({
        {"src", true, true, 0},
        {"main.cpp", false, false, 1},
    });
    ws.set_sidebar(sidebar);
    ws.set_file_tree(file_tree);

    terminal::TerminalScreen screen;
    screen.resize(140, 30);
    ws.render(screen, {0, 0, 140, 30});
    EXPECT_EQ(ws.current_mode(), LayoutMode::TriplePane);
}

TEST(WorkspaceTest, RenderEmptyAreaDoesNotCrash) {
    Workspace ws;
    terminal::TerminalScreen screen;
    ws.render(screen, {0, 0, 0, 0});
}

TEST(WorkspaceTest, RenderDualPaneSidebarHidden) {
    Workspace ws;
    auto sidebar = std::make_shared<ContextSidebar>();
    ws.set_sidebar(sidebar);
    ws.toggle_sidebar(); // Hide sidebar

    terminal::TerminalScreen screen;
    screen.resize(100, 24);
    ws.render(screen, {0, 0, 100, 24});
    EXPECT_EQ(ws.current_mode(), LayoutMode::DualPane);
}

TEST(WorkspaceTest, RenderTriplePaneSidebarHidden) {
    Workspace ws;
    auto sidebar = std::make_shared<ContextSidebar>();
    auto file_tree = std::make_shared<FileTree>();
    ws.set_sidebar(sidebar);
    ws.set_file_tree(file_tree);
    ws.toggle_sidebar();

    terminal::TerminalScreen screen;
    screen.resize(140, 30);
    ws.render(screen, {0, 0, 140, 30});
    EXPECT_EQ(ws.current_mode(), LayoutMode::TriplePane);
}

TEST(WorkspaceTest, HandleEventDelegatesToChat) {
    Workspace ws;
    // Without any child widgets, events are not consumed
    Event key{EventType::Key, 'a'};
    EXPECT_FALSE(ws.handle_event(key));
}

TEST(WorkspaceTest, SetColorSystem) {
    Workspace ws;
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});
    ws.set_color_system(&cs);
    // Just verifying no crash
}

TEST(WorkspaceTest, FullWorkspaceRender) {
    // Integration test: all 3 panes populated
    Workspace ws;

    auto file_tree = std::make_shared<FileTree>();
    file_tree->set_entries({
        {"src", true, true, 0},
        {"cmd", true, false, 1},
        {"tui", true, false, 1},
        {"main.cpp", false, false, 1},
    });

    auto sidebar = std::make_shared<ContextSidebar>();
    sidebar->set_git_branch("main");
    sidebar->set_modified_count(3);
    sidebar->set_model("claude-sonnet-4-6");
    sidebar->set_provider("anthropic");
    sidebar->set_token_count(2400);
    sidebar->set_cost(0.02);
    sidebar->set_agent("code-agent");

    ws.set_file_tree(file_tree);
    ws.set_sidebar(sidebar);

    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});
    ws.set_color_system(&cs);

    terminal::TerminalScreen screen;
    screen.resize(150, 35);
    ws.render(screen, {0, 0, 150, 35});
    EXPECT_EQ(ws.current_mode(), LayoutMode::TriplePane);
}

} // namespace
} // namespace euxis::cli::tui
