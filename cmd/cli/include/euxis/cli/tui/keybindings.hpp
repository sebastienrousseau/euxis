#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace euxis::cli::tui {

/// Semantic actions that can be bound to keys.
enum class Action {
    Submit,          // Enter: submit input
    Cancel,          // Escape: cancel current operation
    AcceptGhost,     // Tab/Right: accept ghost text prediction
    DeleteChar,      // Backspace: delete character
    DeleteWord,      // Ctrl+W: delete word
    MoveUp,          // Arrow Up / k
    MoveDown,        // Arrow Down / j
    MoveLeft,        // Arrow Left / h
    MoveRight,       // Arrow Right / l
    PageUp,          // PgUp / Ctrl+U
    PageDown,        // PgDn / Ctrl+D
    Home,            // Home / Ctrl+A (vim: gg)
    End,             // End / Ctrl+E (vim: G)
    OpenPalette,     // Ctrl+K / Ctrl+P
    OpenFleet,       // Ctrl+F
    OpenPlaybooks,   // Ctrl+R
    ToggleSidebar,   // Ctrl+B
    SwitchAgent,     // Ctrl+A
    SwitchModel,     // Ctrl+M
    ToggleTheme,     // Ctrl+T
    ClearHistory,    // Ctrl+L
    Quit,            // Ctrl+C / Ctrl+D
    FocusNext,       // Tab
    FocusPrev,       // Shift+Tab
    Delete,          // Delete key
    SelectAll,       // Ctrl+A (context: input)
    Copy,            // Ctrl+C (context: selection)
    Paste,           // Ctrl+V
};

/// Well-known key codes (matching read_key() return values).
namespace KeyCode {
    constexpr int ArrowUp    = 1001;
    constexpr int ArrowDown  = 1002;
    constexpr int ArrowRight = 1003;
    constexpr int ArrowLeft  = 1004;
    constexpr int Home       = 1010;
    constexpr int End        = 1011;
    constexpr int PageUp     = 1012;
    constexpr int PageDown   = 1013;
    constexpr int Delete     = 1014;

    // Ctrl+Arrow keys
    constexpr int CtrlUp     = 1101;
    constexpr int CtrlDown   = 1102;
    constexpr int CtrlRight  = 1103;
    constexpr int CtrlLeft   = 1104;

    // Shift+Arrow keys
    constexpr int ShiftUp    = 1201;
    constexpr int ShiftDown  = 1202;
    constexpr int ShiftRight = 1203;
    constexpr int ShiftLeft  = 1204;

    // F-keys
    constexpr int F5  = 1020;
    constexpr int F6  = 1021;
    constexpr int F7  = 1022;
    constexpr int F8  = 1023;
    constexpr int F9  = 1024;
    constexpr int F10 = 1025;
    constexpr int F11 = 1026;
    constexpr int F12 = 1027;

    // Alt+key range: 2000 + ascii
    constexpr int alt_key(char c) { return 2000 + c; }

    // Ctrl+key: the raw byte value
    constexpr int ctrl_key(char c) { return c & 0x1f; }

    constexpr int Enter     = '\r';
    constexpr int Escape    = 27;
    constexpr int Tab       = '\t';
    constexpr int Backspace = 127;
    constexpr int BackspaceAlt = 8;

    // Shift+Tab (CSI Z)
    constexpr int ShiftTab  = 1300;
} // namespace KeyCode

/// Configurable keybinding engine.
/// Maps raw key codes to semantic actions. Supports default, vim, and custom configs.
class KeybindingEngine {
public:
    /// Load keybindings from a JSON/YAML config file.
    /// Config format: { "bindings": { "Ctrl+K": "OpenPalette", ... } }
    bool load(const std::filesystem::path& config_path);

    /// Load the default keybinding preset.
    void load_defaults();

    /// Load the vim keybinding preset (hjkl, Ctrl+U/D, etc.).
    void load_vim_preset();

    /// Resolve a raw key code to an action. Returns nullopt if no binding exists.
    [[nodiscard]] std::optional<Action> resolve(int raw_key) const;

    /// Bind a key to an action.
    void bind(int raw_key, Action action);

    /// Unbind a key.
    void unbind(int raw_key);

    /// Get all current bindings.
    [[nodiscard]] const std::unordered_map<int, Action>& bindings() const { return bindings_; }

    /// Get the display name for a key code (e.g., "Ctrl+K", "Arrow Up").
    [[nodiscard]] static std::string key_name(int raw_key);

    /// Get the display name for an action.
    [[nodiscard]] static std::string action_name(Action action);

    /// Parse a key name string to a key code (e.g., "Ctrl+K" -> 11).
    [[nodiscard]] static std::optional<int> parse_key_name(const std::string& name);

    /// Parse an action name string to an Action enum.
    [[nodiscard]] static std::optional<Action> parse_action_name(const std::string& name);

private:
    std::unordered_map<int, Action> bindings_;
};

} // namespace euxis::cli::tui
