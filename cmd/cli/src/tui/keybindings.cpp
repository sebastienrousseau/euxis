#include "euxis/cli/tui/keybindings.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// Default keybindings
// ---------------------------------------------------------------------------

void KeybindingEngine::load_defaults() {
    bindings_.clear();

    // Navigation
    bind(KeyCode::ArrowUp, Action::MoveUp);
    bind(KeyCode::ArrowDown, Action::MoveDown);
    bind(KeyCode::ArrowLeft, Action::MoveLeft);
    bind(KeyCode::ArrowRight, Action::MoveRight);
    bind(KeyCode::PageUp, Action::PageUp);
    bind(KeyCode::PageDown, Action::PageDown);
    bind(KeyCode::Home, Action::Home);
    bind(KeyCode::End, Action::End);

    // Editing
    bind(KeyCode::Enter, Action::Submit);
    bind(KeyCode::Escape, Action::Cancel);
    bind(KeyCode::Tab, Action::AcceptGhost);
    bind(KeyCode::Backspace, Action::DeleteChar);
    bind(KeyCode::BackspaceAlt, Action::DeleteChar);
    bind(KeyCode::Delete, Action::Delete);
    bind(KeyCode::ctrl_key('w'), Action::DeleteWord);

    // Commands
    bind(KeyCode::ctrl_key('k'), Action::OpenPalette);
    bind(KeyCode::ctrl_key('p'), Action::OpenPalette);
    bind(KeyCode::ctrl_key('f'), Action::OpenFleet);
    bind(KeyCode::ctrl_key('r'), Action::OpenPlaybooks);
    bind(KeyCode::ctrl_key('b'), Action::ToggleSidebar);
    bind(KeyCode::ctrl_key('l'), Action::ClearHistory);
    bind(KeyCode::ctrl_key('t'), Action::ToggleTheme);

    // Quit
    bind(KeyCode::ctrl_key('c'), Action::Quit);
    bind(KeyCode::ctrl_key('d'), Action::Quit);

    // Focus
    bind(KeyCode::ShiftTab, Action::FocusPrev);

    // Clipboard
    bind(KeyCode::ctrl_key('v'), Action::Paste);
}

// ---------------------------------------------------------------------------
// Vim preset
// ---------------------------------------------------------------------------

void KeybindingEngine::load_vim_preset() {
    load_defaults(); // Start with defaults, then overlay vim keys

    // vim navigation (normal mode context)
    bind(static_cast<int>('k'), Action::MoveUp);
    bind(static_cast<int>('j'), Action::MoveDown);
    bind(static_cast<int>('h'), Action::MoveLeft);
    bind(static_cast<int>('l'), Action::MoveRight);

    // vim scrolling
    bind(KeyCode::ctrl_key('u'), Action::PageUp);
    bind(KeyCode::ctrl_key('d'), Action::PageDown);

    // vim begin/end
    bind(static_cast<int>('g'), Action::Home);
    bind(static_cast<int>('G'), Action::End);
}

// ---------------------------------------------------------------------------
// Config loading (JSON format)
// ---------------------------------------------------------------------------

bool KeybindingEngine::load(const std::filesystem::path& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j;
        file >> j;

        if (!j.contains("bindings") || !j["bindings"].is_object()) return false;

        // Start from defaults
        load_defaults();

        for (auto& [key_str, action_str] : j["bindings"].items()) {
            auto key = parse_key_name(key_str);
            auto action = parse_action_name(action_str.get<std::string>());
            if (key && action) {
                bind(*key, *action);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// Core operations
// ---------------------------------------------------------------------------

std::optional<Action> KeybindingEngine::resolve(int raw_key) const {
    auto it = bindings_.find(raw_key);
    if (it != bindings_.end()) return it->second;
    return std::nullopt;
}

void KeybindingEngine::bind(int raw_key, Action action) {
    bindings_[raw_key] = action;
}

void KeybindingEngine::unbind(int raw_key) {
    bindings_.erase(raw_key);
}

// ---------------------------------------------------------------------------
// Name conversion
// ---------------------------------------------------------------------------

std::string KeybindingEngine::key_name(int raw_key) {
    // Special keys
    if (raw_key == KeyCode::ArrowUp)    return "Arrow Up";
    if (raw_key == KeyCode::ArrowDown)  return "Arrow Down";
    if (raw_key == KeyCode::ArrowLeft)  return "Arrow Left";
    if (raw_key == KeyCode::ArrowRight) return "Arrow Right";
    if (raw_key == KeyCode::Home)       return "Home";
    if (raw_key == KeyCode::End)        return "End";
    if (raw_key == KeyCode::PageUp)     return "Page Up";
    if (raw_key == KeyCode::PageDown)   return "Page Down";
    if (raw_key == KeyCode::Delete)     return "Delete";
    if (raw_key == KeyCode::Enter)      return "Enter";
    if (raw_key == KeyCode::Escape)     return "Escape";
    if (raw_key == KeyCode::Tab)        return "Tab";
    if (raw_key == KeyCode::ShiftTab)   return "Shift+Tab";
    if (raw_key == KeyCode::Backspace)  return "Backspace";

    // Ctrl+key
    if (raw_key >= 1 && raw_key <= 26) {
        return "Ctrl+" + std::string(1, static_cast<char>('A' + raw_key - 1));
    }

    // Alt+key
    if (raw_key >= 2000 && raw_key < 2128) {
        return "Alt+" + std::string(1, static_cast<char>(raw_key - 2000));
    }

    // F-keys
    if (raw_key >= KeyCode::F5 && raw_key <= KeyCode::F12) {
        return "F" + std::to_string(raw_key - KeyCode::F5 + 5);
    }

    // Printable ASCII
    if (raw_key >= 32 && raw_key <= 126) {
        return std::string(1, static_cast<char>(raw_key));
    }

    return "Key(" + std::to_string(raw_key) + ")";
}

std::string KeybindingEngine::action_name(Action action) {
    switch (action) {
        case Action::Submit:        return "Submit";
        case Action::Cancel:        return "Cancel";
        case Action::AcceptGhost:   return "AcceptGhost";
        case Action::DeleteChar:    return "DeleteChar";
        case Action::DeleteWord:    return "DeleteWord";
        case Action::MoveUp:        return "MoveUp";
        case Action::MoveDown:      return "MoveDown";
        case Action::MoveLeft:      return "MoveLeft";
        case Action::MoveRight:     return "MoveRight";
        case Action::PageUp:        return "PageUp";
        case Action::PageDown:      return "PageDown";
        case Action::Home:          return "Home";
        case Action::End:           return "End";
        case Action::OpenPalette:   return "OpenPalette";
        case Action::OpenFleet:     return "OpenFleet";
        case Action::OpenPlaybooks: return "OpenPlaybooks";
        case Action::ToggleSidebar: return "ToggleSidebar";
        case Action::SwitchAgent:   return "SwitchAgent";
        case Action::SwitchModel:   return "SwitchModel";
        case Action::ToggleTheme:   return "ToggleTheme";
        case Action::ClearHistory:  return "ClearHistory";
        case Action::Quit:          return "Quit";
        case Action::FocusNext:     return "FocusNext";
        case Action::FocusPrev:     return "FocusPrev";
        case Action::Delete:        return "Delete";
        case Action::SelectAll:     return "SelectAll";
        case Action::Copy:          return "Copy";
        case Action::Paste:         return "Paste";
    }
    return "Unknown";
}

std::optional<int> KeybindingEngine::parse_key_name(const std::string& name) {
    if (name == "Enter") return KeyCode::Enter;
    if (name == "Escape") return KeyCode::Escape;
    if (name == "Tab") return KeyCode::Tab;
    if (name == "Shift+Tab") return KeyCode::ShiftTab;
    if (name == "Backspace") return KeyCode::Backspace;
    if (name == "Delete") return KeyCode::Delete;
    if (name == "Home") return KeyCode::Home;
    if (name == "End") return KeyCode::End;
    if (name == "Page Up") return KeyCode::PageUp;
    if (name == "Page Down") return KeyCode::PageDown;
    if (name == "Arrow Up") return KeyCode::ArrowUp;
    if (name == "Arrow Down") return KeyCode::ArrowDown;
    if (name == "Arrow Left") return KeyCode::ArrowLeft;
    if (name == "Arrow Right") return KeyCode::ArrowRight;

    // Ctrl+X
    if (name.starts_with("Ctrl+") && name.size() == 6) {
        char c = name[5];
        if (c >= 'A' && c <= 'Z') return KeyCode::ctrl_key(c);
        if (c >= 'a' && c <= 'z') return KeyCode::ctrl_key(static_cast<char>(c - 32));
    }

    // Alt+X
    if (name.starts_with("Alt+") && name.size() == 5) {
        return KeyCode::alt_key(name[4]);
    }

    // F-keys
    if (name.starts_with("F") && name.size() >= 2) {
        try {
            int n = std::stoi(name.substr(1));
            if (n >= 5 && n <= 12) return KeyCode::F5 + (n - 5);
        } catch (...) {}
    }

    // Single character
    if (name.size() == 1 && name[0] >= 32 && name[0] <= 126) {
        return static_cast<int>(name[0]);
    }

    return std::nullopt;
}

std::optional<Action> KeybindingEngine::parse_action_name(const std::string& name) {
    static const std::unordered_map<std::string, Action> map = {
        {"Submit", Action::Submit}, {"Cancel", Action::Cancel},
        {"AcceptGhost", Action::AcceptGhost}, {"DeleteChar", Action::DeleteChar},
        {"DeleteWord", Action::DeleteWord}, {"MoveUp", Action::MoveUp},
        {"MoveDown", Action::MoveDown}, {"MoveLeft", Action::MoveLeft},
        {"MoveRight", Action::MoveRight}, {"PageUp", Action::PageUp},
        {"PageDown", Action::PageDown}, {"Home", Action::Home},
        {"End", Action::End}, {"OpenPalette", Action::OpenPalette},
        {"OpenFleet", Action::OpenFleet}, {"OpenPlaybooks", Action::OpenPlaybooks},
        {"ToggleSidebar", Action::ToggleSidebar},
        {"SwitchAgent", Action::SwitchAgent}, {"SwitchModel", Action::SwitchModel},
        {"ToggleTheme", Action::ToggleTheme}, {"ClearHistory", Action::ClearHistory},
        {"Quit", Action::Quit}, {"FocusNext", Action::FocusNext},
        {"FocusPrev", Action::FocusPrev}, {"Delete", Action::Delete},
        {"SelectAll", Action::SelectAll}, {"Copy", Action::Copy},
        {"Paste", Action::Paste},
    };
    auto it = map.find(name);
    if (it != map.end()) return it->second;
    return std::nullopt;
}

} // namespace euxis::cli::tui
