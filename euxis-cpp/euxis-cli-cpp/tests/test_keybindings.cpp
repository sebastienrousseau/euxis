#include <gtest/gtest.h>
#include "euxis/cli/tui/keybindings.hpp"

#include <filesystem>
#include <fstream>

namespace euxis::cli::tui {
namespace {

// --- Default Bindings ---

TEST(KeybindingsTest, DefaultsArrowKeys) {
    KeybindingEngine kb;
    kb.load_defaults();

    EXPECT_EQ(kb.resolve(KeyCode::ArrowUp), Action::MoveUp);
    EXPECT_EQ(kb.resolve(KeyCode::ArrowDown), Action::MoveDown);
    EXPECT_EQ(kb.resolve(KeyCode::ArrowLeft), Action::MoveLeft);
    EXPECT_EQ(kb.resolve(KeyCode::ArrowRight), Action::MoveRight);
}

TEST(KeybindingsTest, DefaultsEnterEscape) {
    KeybindingEngine kb;
    kb.load_defaults();

    EXPECT_EQ(kb.resolve(KeyCode::Enter), Action::Submit);
    EXPECT_EQ(kb.resolve(KeyCode::Escape), Action::Cancel);
}

TEST(KeybindingsTest, DefaultsCtrlKeys) {
    KeybindingEngine kb;
    kb.load_defaults();

    EXPECT_EQ(kb.resolve(KeyCode::ctrl_key('k')), Action::OpenPalette);
    EXPECT_EQ(kb.resolve(KeyCode::ctrl_key('p')), Action::OpenPalette);
    EXPECT_EQ(kb.resolve(KeyCode::ctrl_key('c')), Action::Quit);
    EXPECT_EQ(kb.resolve(KeyCode::ctrl_key('d')), Action::Quit);
    EXPECT_EQ(kb.resolve(KeyCode::ctrl_key('w')), Action::DeleteWord);
}

TEST(KeybindingsTest, DefaultsNavigationKeys) {
    KeybindingEngine kb;
    kb.load_defaults();

    EXPECT_EQ(kb.resolve(KeyCode::PageUp), Action::PageUp);
    EXPECT_EQ(kb.resolve(KeyCode::PageDown), Action::PageDown);
    EXPECT_EQ(kb.resolve(KeyCode::Home), Action::Home);
    EXPECT_EQ(kb.resolve(KeyCode::End), Action::End);
    EXPECT_EQ(kb.resolve(KeyCode::Delete), Action::Delete);
}

TEST(KeybindingsTest, DefaultsTab) {
    KeybindingEngine kb;
    kb.load_defaults();

    EXPECT_EQ(kb.resolve(KeyCode::Tab), Action::AcceptGhost);
    EXPECT_EQ(kb.resolve(KeyCode::ShiftTab), Action::FocusPrev);
}

TEST(KeybindingsTest, UnboundKeyReturnsNullopt) {
    KeybindingEngine kb;
    kb.load_defaults();

    EXPECT_EQ(kb.resolve(9999), std::nullopt);
    EXPECT_EQ(kb.resolve(static_cast<int>('z')), std::nullopt);
}

// --- Vim Preset ---

TEST(KeybindingsTest, VimPresetHJKL) {
    KeybindingEngine kb;
    kb.load_vim_preset();

    EXPECT_EQ(kb.resolve(static_cast<int>('h')), Action::MoveLeft);
    EXPECT_EQ(kb.resolve(static_cast<int>('j')), Action::MoveDown);
    EXPECT_EQ(kb.resolve(static_cast<int>('k')), Action::MoveUp);
    EXPECT_EQ(kb.resolve(static_cast<int>('l')), Action::MoveRight);
}

TEST(KeybindingsTest, VimPresetScrolling) {
    KeybindingEngine kb;
    kb.load_vim_preset();

    EXPECT_EQ(kb.resolve(KeyCode::ctrl_key('u')), Action::PageUp);
    EXPECT_EQ(kb.resolve(KeyCode::ctrl_key('d')), Action::PageDown);
}

TEST(KeybindingsTest, VimPresetGG) {
    KeybindingEngine kb;
    kb.load_vim_preset();

    EXPECT_EQ(kb.resolve(static_cast<int>('g')), Action::Home);
    EXPECT_EQ(kb.resolve(static_cast<int>('G')), Action::End);
}

TEST(KeybindingsTest, VimStillHasDefaults) {
    KeybindingEngine kb;
    kb.load_vim_preset();

    // Vim preset overlays defaults, so Enter/Escape should still work
    EXPECT_EQ(kb.resolve(KeyCode::Enter), Action::Submit);
    EXPECT_EQ(kb.resolve(KeyCode::Escape), Action::Cancel);
}

// --- Custom Binding ---

TEST(KeybindingsTest, CustomBind) {
    KeybindingEngine kb;
    kb.load_defaults();

    kb.bind(static_cast<int>('x'), Action::Quit);
    EXPECT_EQ(kb.resolve(static_cast<int>('x')), Action::Quit);
}

TEST(KeybindingsTest, Unbind) {
    KeybindingEngine kb;
    kb.load_defaults();

    kb.unbind(KeyCode::ArrowUp);
    EXPECT_EQ(kb.resolve(KeyCode::ArrowUp), std::nullopt);
}

TEST(KeybindingsTest, OverrideBind) {
    KeybindingEngine kb;
    kb.load_defaults();

    // Override Enter from Submit to Cancel
    kb.bind(KeyCode::Enter, Action::Cancel);
    EXPECT_EQ(kb.resolve(KeyCode::Enter), Action::Cancel);
}

// --- Key Name Conversion ---

TEST(KeybindingsTest, KeyNameSpecialKeys) {
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::ArrowUp), "Arrow Up");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::ArrowDown), "Arrow Down");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::Enter), "Enter");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::Escape), "Escape");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::Tab), "Tab");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::ShiftTab), "Shift+Tab");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::Backspace), "Backspace");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::Home), "Home");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::End), "End");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::PageUp), "Page Up");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::PageDown), "Page Down");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::Delete), "Delete");
}

TEST(KeybindingsTest, KeyNameCtrl) {
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::ctrl_key('K')), "Ctrl+K");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::ctrl_key('A')), "Ctrl+A");
}

TEST(KeybindingsTest, KeyNameAlt) {
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::alt_key('x')), "Alt+x");
}

TEST(KeybindingsTest, KeyNameFKey) {
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::F5), "F5");
    EXPECT_EQ(KeybindingEngine::key_name(KeyCode::F12), "F12");
}

TEST(KeybindingsTest, KeyNamePrintable) {
    EXPECT_EQ(KeybindingEngine::key_name(static_cast<int>('a')), "a");
    EXPECT_EQ(KeybindingEngine::key_name(static_cast<int>('Z')), "Z");
}

// --- Action Name ---

TEST(KeybindingsTest, ActionNameRoundTrip) {
    for (int i = 0; i <= static_cast<int>(Action::Paste); ++i) {
        auto action = static_cast<Action>(i);
        std::string name = KeybindingEngine::action_name(action);
        EXPECT_FALSE(name.empty());
        EXPECT_NE(name, "Unknown");

        auto parsed = KeybindingEngine::parse_action_name(name);
        EXPECT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, action);
    }
}

// --- Parse Key Name ---

TEST(KeybindingsTest, ParseKeyNameSpecial) {
    EXPECT_EQ(KeybindingEngine::parse_key_name("Enter"), KeyCode::Enter);
    EXPECT_EQ(KeybindingEngine::parse_key_name("Escape"), KeyCode::Escape);
    EXPECT_EQ(KeybindingEngine::parse_key_name("Tab"), KeyCode::Tab);
    EXPECT_EQ(KeybindingEngine::parse_key_name("Shift+Tab"), KeyCode::ShiftTab);
    EXPECT_EQ(KeybindingEngine::parse_key_name("Home"), KeyCode::Home);
}

TEST(KeybindingsTest, ParseKeyNameCtrl) {
    EXPECT_EQ(KeybindingEngine::parse_key_name("Ctrl+K"), KeyCode::ctrl_key('K'));
    EXPECT_EQ(KeybindingEngine::parse_key_name("Ctrl+A"), KeyCode::ctrl_key('A'));
}

TEST(KeybindingsTest, ParseKeyNameAlt) {
    EXPECT_EQ(KeybindingEngine::parse_key_name("Alt+x"), KeyCode::alt_key('x'));
}

TEST(KeybindingsTest, ParseKeyNameFKey) {
    EXPECT_EQ(KeybindingEngine::parse_key_name("F5"), KeyCode::F5);
    EXPECT_EQ(KeybindingEngine::parse_key_name("F12"), KeyCode::F12);
}

TEST(KeybindingsTest, ParseKeyNameSingleChar) {
    EXPECT_EQ(KeybindingEngine::parse_key_name("a"), static_cast<int>('a'));
}

TEST(KeybindingsTest, ParseKeyNameInvalid) {
    EXPECT_EQ(KeybindingEngine::parse_key_name(""), std::nullopt);
    EXPECT_EQ(KeybindingEngine::parse_key_name("InvalidKey"), std::nullopt);
}

// --- Config Loading ---

TEST(KeybindingsTest, LoadFromJsonFile) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_test_keybindings.json";
    {
        std::ofstream f(tmp);
        f << R"({
            "bindings": {
                "Ctrl+K": "OpenFleet",
                "F5": "ToggleTheme"
            }
        })";
    }

    KeybindingEngine kb;
    EXPECT_TRUE(kb.load(tmp));

    EXPECT_EQ(kb.resolve(KeyCode::ctrl_key('K')), Action::OpenFleet);
    EXPECT_EQ(kb.resolve(KeyCode::F5), Action::ToggleTheme);

    // Defaults should still work for unbound keys
    EXPECT_EQ(kb.resolve(KeyCode::Enter), Action::Submit);

    std::filesystem::remove(tmp);
}

TEST(KeybindingsTest, LoadFromNonexistentFile) {
    KeybindingEngine kb;
    EXPECT_FALSE(kb.load("/nonexistent/path.json"));
}

TEST(KeybindingsTest, LoadFromInvalidJson) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_test_bad_keybindings.json";
    {
        std::ofstream f(tmp);
        f << "not valid json";
    }

    KeybindingEngine kb;
    EXPECT_FALSE(kb.load(tmp));

    std::filesystem::remove(tmp);
}

TEST(KeybindingsTest, BindingsMapAccessible) {
    KeybindingEngine kb;
    kb.load_defaults();
    EXPECT_GT(kb.bindings().size(), 0u);
}

} // namespace
} // namespace euxis::cli::tui
