#include <gtest/gtest.h>
#include "euxis/cli/tui/swarm_view.hpp"

namespace euxis::cli::tui {
namespace {

SwarmNode make_council_tree() {
    SwarmNode root;
    root.agent_id = "council";
    root.status = "running";
    root.active = true;
    root.children = {
        {"architect", "analyzing...", true, true, 3.2, {}},
        {"security", "scanning...", true, true, 2.1, {}},
        {"code", "waiting...", false, true, 0.0, {}},
        {"qa", "waiting...", false, true, 0.0, {}},
    };
    return root;
}

TEST(SwarmViewTest, ConstructDefault) {
    SwarmView sv;
    EXPECT_TRUE(sv.focusable());
    EXPECT_FALSE(sv.accessible_name.empty());
}

TEST(SwarmViewTest, SetName) {
    SwarmView sv;
    sv.set_name("API design review");
    // Just verifying no crash
}

TEST(SwarmViewTest, SetRoot) {
    SwarmView sv;
    sv.set_root(make_council_tree());
}

TEST(SwarmViewTest, PreferredSizeEmpty) {
    SwarmView sv;
    auto sz = sv.preferred_size();
    EXPECT_EQ(sz.width, 60);
    EXPECT_GE(sz.height, 1); // At least root node
}

TEST(SwarmViewTest, PreferredSizeWithChildren) {
    SwarmView sv;
    sv.set_root(make_council_tree());
    auto sz = sv.preferred_size();
    EXPECT_EQ(sz.width, 60);
    EXPECT_GE(sz.height, 5); // root + 4 children
}

TEST(SwarmViewTest, RenderDoesNotCrash) {
    SwarmView sv;
    sv.set_name("API design review");
    sv.set_root(make_council_tree());

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 10});
}

TEST(SwarmViewTest, RenderEmptyTree) {
    SwarmView sv;
    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 10});
}

TEST(SwarmViewTest, RenderEmptyArea) {
    SwarmView sv;
    sv.set_root(make_council_tree());
    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 0, 0});
}

TEST(SwarmViewTest, RenderWithColorSystem) {
    SwarmView sv;
    sv.set_name("Security audit");
    sv.set_root(make_council_tree());

    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});
    sv.set_color_system(&cs);

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 15});
}

TEST(SwarmViewTest, RenderTruncatedHeight) {
    SwarmView sv;
    sv.set_root(make_council_tree());

    terminal::TerminalScreen screen;
    // Height too small to fit all nodes
    sv.render(screen, {0, 0, 80, 3});
}

TEST(SwarmViewTest, DeepNestedTree) {
    SwarmNode root;
    root.agent_id = "orchestrator";
    root.status = "running";
    root.active = true;

    SwarmNode squad1;
    squad1.agent_id = "squad-alpha";
    squad1.status = "running";
    squad1.active = true;
    squad1.children = {
        {"code", "writing...", true, true, 1.5, {}},
        {"test", "waiting...", false, true, 0.0, {}},
    };

    SwarmNode squad2;
    squad2.agent_id = "squad-beta";
    squad2.status = "waiting";
    squad2.active = false;
    squad2.children = {
        {"review", "queued", false, true, 0.0, {}},
    };

    root.children = {squad1, squad2};

    SwarmView sv;
    sv.set_name("Multi-squad deployment");
    sv.set_root(root);

    auto sz = sv.preferred_size();
    EXPECT_GE(sz.height, 6); // root + 2 squads + 3 agents

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 20});
}

TEST(SwarmViewTest, NodeWithErrorStatus) {
    SwarmNode root;
    root.agent_id = "council";
    root.status = "running";
    root.active = true;
    root.children = {
        {"code", "error", false, true, 5.0, {}},
        {"review", "done", false, true, 3.0, {}},
    };

    SwarmView sv;
    sv.set_root(root);

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 10});
}

TEST(SwarmViewTest, NodeWithDoneStatus) {
    SwarmNode root;
    root.agent_id = "council";
    root.status = "done";
    root.active = false;
    root.children = {
        {"code", "done", false, true, 5.0, {}},
        {"review", "done", false, true, 3.0, {}},
    };

    SwarmView sv;
    sv.set_root(root);

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 10});
}

TEST(SwarmViewTest, CollapsibleNodes) {
    SwarmNode root;
    root.agent_id = "orchestrator";
    root.expanded = false;
    root.children = {
        {"agent1", "waiting", false, true, 0.0, {}},
        {"agent2", "waiting", false, true, 0.0, {}},
    };

    SwarmView sv;
    sv.set_root(root);

    auto sz = sv.preferred_size();
    // Only root is visible (1 line) + header (1 line) = 2
    EXPECT_EQ(sz.height, 2);

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 10});
}

TEST(SwarmViewTest, ActiveNodeShowsElapsed) {
    SwarmNode root;
    root.agent_id = "council";
    root.status = "running";
    root.active = true;
    root.elapsed_s = 10.5;
    root.children = {
        {"agent", "processing...", true, true, 8.3, {}},
    };

    SwarmView sv;
    sv.set_root(root);

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 10});
}

TEST(SwarmViewTest, InactiveNodeHidesElapsed) {
    SwarmNode root;
    root.agent_id = "council";
    root.status = "waiting";
    root.active = false;
    root.elapsed_s = 0.0;

    SwarmView sv;
    sv.set_root(root);

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 10});
}

TEST(SwarmViewTest, CountVisibleLines) {
    SwarmView sv;
    sv.set_root(make_council_tree());
    auto sz = sv.preferred_size();
    // root (1) + 4 children = 5, +1 for header line
    EXPECT_EQ(sz.height, 6);
}

TEST(SwarmViewTest, AccessibleName) {
    SwarmView sv;
    EXPECT_EQ(sv.accessible_name, "Swarm View");
}

TEST(SwarmViewTest, AccessibleDescription) {
    SwarmView sv;
    EXPECT_NE(sv.accessible_description.find("orchestration"), std::string::npos);
}

TEST(SwarmViewTest, SingleChildLastNode) {
    SwarmNode root;
    root.agent_id = "solo";
    root.status = "running";
    root.active = true;
    root.children = {
        {"agent", "working...", true, true, 1.0, {}},
    };

    SwarmView sv;
    sv.set_root(root);

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 10});
}

TEST(SwarmViewTest, EmptyAgentId) {
    SwarmNode root;
    root.status = "running";
    root.active = true;

    SwarmView sv;
    sv.set_root(root);

    terminal::TerminalScreen screen;
    sv.render(screen, {0, 0, 80, 5});
}

} // namespace
} // namespace euxis::cli::tui
