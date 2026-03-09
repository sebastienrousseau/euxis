#include <gtest/gtest.h>
#include "euxis/cli/tui/palette.hpp"

namespace euxis::cli::tui {
namespace {

// --- FuzzyMatch Tests ---

TEST(FuzzyMatchTest, EmptyQueryMatchesAll) {
    EXPECT_GT(FuzzyMatch::score("", "anything"), 0);
}

TEST(FuzzyMatchTest, EmptyCandidateNoMatch) {
    EXPECT_EQ(FuzzyMatch::score("foo", ""), 0);
}

TEST(FuzzyMatchTest, ExactPrefixHighest) {
    int exact = FuzzyMatch::score("/he", "/help");
    int partial = FuzzyMatch::score("el", "/help");
    EXPECT_GT(exact, partial);
}

TEST(FuzzyMatchTest, FullMatch) {
    EXPECT_EQ(FuzzyMatch::score("/help", "/help"), 100);
}

TEST(FuzzyMatchTest, SubsequenceMatch) {
    int s = FuzzyMatch::score("hlp", "/help");
    EXPECT_GT(s, 0);
}

TEST(FuzzyMatchTest, NoMatch) {
    EXPECT_EQ(FuzzyMatch::score("xyz", "abc"), 0);
}

TEST(FuzzyMatchTest, CaseInsensitive) {
    int s1 = FuzzyMatch::score("HELP", "/help");
    EXPECT_GT(s1, 0);
}

TEST(FuzzyMatchTest, WordBoundaryBonus) {
    int boundary = FuzzyMatch::score("ca", "code-agent");
    int plain = FuzzyMatch::score("ca", "xyzca");
    EXPECT_GE(boundary, plain);
}

TEST(FuzzyMatchTest, MatchPositions) {
    auto pos = FuzzyMatch::match_positions("fl", "fleet");
    ASSERT_EQ(pos.size(), 2u);
    EXPECT_EQ(pos[0], 0u);
    EXPECT_EQ(pos[1], 1u);
}

TEST(FuzzyMatchTest, MatchPositionsSubsequence) {
    auto pos = FuzzyMatch::match_positions("ft", "fleet");
    ASSERT_EQ(pos.size(), 2u);
    EXPECT_EQ(pos[0], 0u); // 'f' at 0
    EXPECT_EQ(pos[1], 4u); // 't' at 4
}

// --- Palette Tests ---

TEST(PaletteTest, ConstructDestruct) {
    Palette p;
    EXPECT_TRUE(p.query().empty());
    EXPECT_TRUE(p.filtered().empty());
    EXPECT_TRUE(p.focusable());
}

TEST(PaletteTest, SetEntries) {
    Palette p;
    std::vector<PaletteEntry> entries = {
        {"help", "Command", "Show help"},
        {"fleet", "Command", "Fleet browser"},
        {"code-agent", "Agent", "Code assistant"},
    };
    p.set_entries(std::move(entries));
    EXPECT_EQ(p.filtered().size(), 3u);
}

TEST(PaletteTest, FilterByQuery) {
    Palette p;
    p.set_entries({
        {"help", "Command", "Show help"},
        {"fleet", "Command", "Fleet browser"},
        {"code-agent", "Agent", "Code assistant"},
    });

    // Simulate typing 'f'
    Event ev{EventType::Key, 'f'};
    p.handle_event(ev);

    EXPECT_EQ(p.query(), "f");
    // "fleet" should match, "help" shouldn't start with f
    EXPECT_GT(p.filtered().size(), 0u);
    bool has_fleet = false;
    for (const auto& e : p.filtered()) {
        if (e.label == "fleet") has_fleet = true;
    }
    EXPECT_TRUE(has_fleet);
}

TEST(PaletteTest, NavigateUpDown) {
    Palette p;
    p.set_entries({
        {"a", "Cmd", ""},
        {"b", "Cmd", ""},
        {"c", "Cmd", ""},
    });

    EXPECT_EQ(p.selected(), 0);

    Event down{EventType::Key, 1002};
    p.handle_event(down);
    EXPECT_EQ(p.selected(), 1);

    p.handle_event(down);
    EXPECT_EQ(p.selected(), 2);

    // Can't go below last
    p.handle_event(down);
    EXPECT_EQ(p.selected(), 2);

    Event up{EventType::Key, 1001};
    p.handle_event(up);
    EXPECT_EQ(p.selected(), 1);
}

TEST(PaletteTest, SelectCallback) {
    Palette p;
    p.set_entries({{"help", "Command", ""}});

    std::string selected_label;
    p.set_on_select([&](const PaletteEntry& e) { selected_label = e.label; });

    Event enter{EventType::Key, '\r'};
    p.handle_event(enter);

    EXPECT_EQ(selected_label, "help");
}

TEST(PaletteTest, Backspace) {
    Palette p;
    p.set_entries({{"help", "Command", ""}});

    Event h_key{EventType::Key, 'h'};
    Event e_key{EventType::Key, 'e'};
    p.handle_event(h_key);
    p.handle_event(e_key);
    EXPECT_EQ(p.query(), "he");

    Event backspace{EventType::Key, 127};
    p.handle_event(backspace);
    EXPECT_EQ(p.query(), "h");
}

TEST(PaletteTest, Reset) {
    Palette p;
    p.set_entries({{"help", "Command", ""}});

    Event h_key{EventType::Key, 'h'};
    p.handle_event(h_key);
    EXPECT_EQ(p.query(), "h");

    p.reset();
    EXPECT_TRUE(p.query().empty());
    EXPECT_EQ(p.selected(), 0);
}

TEST(PaletteTest, RenderDoesNotCrash) {
    Palette p;
    p.set_entries({{"help", "Command", "Show help"}, {"fleet", "Command", "Browse fleet"}});

    terminal::TerminalScreen screen;
    p.render(screen, {5, 5, 60, 10});
}

TEST(PaletteTest, PreferredSize) {
    Palette p;
    auto sz = p.preferred_size();
    EXPECT_EQ(sz.width, 60);
    EXPECT_GE(sz.height, 2);
}

TEST(PaletteTest, FilteredSortedByScore) {
    Palette p;
    p.set_entries({
        {"exit", "Cmd", ""},
        {"execute", "Cmd", ""},
        {"example", "Cmd", ""},
    });

    Event e{EventType::Key, 'e'};
    Event x{EventType::Key, 'x'};
    p.handle_event(e);
    p.handle_event(x);

    // "exit" should rank highest (exact prefix "ex")
    if (!p.filtered().empty()) {
        EXPECT_EQ(p.filtered()[0].label, "exit");
    }
}

} // namespace
} // namespace euxis::cli::tui
