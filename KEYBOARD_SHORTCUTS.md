# Euxis Keyboard Shortcuts Reference

**Enterprise Unified eXecution Intelligence System**

Version 0.0.6

---

## Global Shortcuts

These shortcuts work across all Euxis interfaces when available:

### Command & Navigation
| Shortcut | Action | Context | Implementation Status |
|----------|--------|---------|----------------------|
| `Ctrl+K` | Open command palette | Global | 🔶 Planned |
| `Ctrl+Shift+P` | Open command palette (alternate) | Global | 🔶 Planned |
| `Ctrl+/` | Show keyboard shortcuts help | Global | 🔶 Planned |
| `F1` | Help/Documentation | Global | 🔶 Planned |
| `Esc` | Close modal/dialog/palette | Overlays | ✅ Partial |

### Focus Management
| Shortcut | Action | Context | Implementation Status |
|----------|--------|---------|----------------------|
| `Tab` | Next focusable element | Global | ✅ Basic |
| `Shift+Tab` | Previous focusable element | Global | ✅ Basic |
| `Arrow Keys` | Navigate within groups | Lists, Menus | ✅ Basic |
| `Enter` | Activate focused element | Interactive | ✅ Basic |
| `Space` | Select/toggle focused element | Checkboxes, Buttons | 🔶 Planned |

---

## euxis-ui (Terminal Interface)

Current keyboard support in the Mission Control TUI:

### Main Menu
| Shortcut | Action | Notes |
|----------|--------|-------|
| `1` | Deploy Agent | Direct menu selection |
| `2` | Switch AI Provider | Direct menu selection |
| `3` | View Agent Logs | Direct menu selection |
| `4` | Change Directory | Direct menu selection |
| `5` | Exit | Direct menu selection |
| `↑/↓` | Navigate menu | When in select mode |
| `Enter` | Confirm selection | When in select mode |
| `Ctrl+C` | Cancel/Return | Any input prompt |

### Agent Selection
| Shortcut | Action | Notes |
|----------|--------|-------|
| `↑/↓` | Navigate agent list | Standard select navigation |
| `Enter` | Choose agent | Proceeds to task input |
| `Cancel` | Return to main menu | Select "Cancel" option |

### Provider Selection
| Shortcut | Action | Notes |
|----------|--------|-------|
| `↑/↓` | Navigate provider list | claude, gemini, openai, ollama, opencode |
| `Enter` | Choose provider | Sets current provider |
| `Cancel` | Keep current provider | Select "Cancel" option |

---

## Command Palette (Planned)

Future command palette will support these patterns:

### Search & Navigation
| Shortcut | Action | Description |
|----------|--------|-------------|
| `Ctrl+K` | Open palette | Primary trigger |
| `↑/↓` | Navigate results | Highlight commands |
| `Enter` | Execute command | Run selected command |
| `Esc` | Close palette | Return focus to previous |
| `Tab` | Navigate categories | Switch between command groups |

### Quick Actions
| Pattern | Description | Examples |
|---------|-------------|----------|
| `> command` | Direct command execution | `> health`, `> lint`, `> certify` |
| `@ agent` | Agent deployment | `@ architect Review auth module` |
| `# squad` | Squad deployment | `# quality Audit payment service` |
| `? help` | Documentation lookup | `? keyboard shortcuts`, `? agents` |
| `! tool` | Utility execution | `! bench`, `! audit-run` |

---

## Agent-Specific Shortcuts

Future agent interfaces may define specialized shortcuts:

### orchestrator
| Shortcut | Action | Context |
|----------|--------|---------|
| `Ctrl+D` | Create dispatch manifest | Planning mode |
| `Ctrl+M` | Switch to mesh mode | Coordination |
| `Ctrl+F` | Switch to federated mode | Coordination |

### architect
| Shortcut | Action | Context |
|----------|--------|---------|
| `Ctrl+P` | Create pattern template | Design mode |
| `Ctrl+R` | Review architecture | Analysis mode |

### edge-hunter
| Shortcut | Action | Context |
|----------|--------|---------|
| `Ctrl+S` | Start security scan | Audit mode |
| `Ctrl+V` | View vulnerability report | Results |

---

## Accessibility Shortcuts

Ensuring keyboard-only operation:

### Screen Reader Support
| Shortcut | Action | Purpose |
|----------|--------|---------|
| `Ctrl+Alt+T` | Toggle TTS mode | Voice feedback |
| `Ctrl+Alt+V` | Verbose mode | Detailed descriptions |
| `Ctrl+Alt+H` | Heading navigation | Skip to sections |

### Visual Accessibility
| Shortcut | Action | Purpose |
|----------|--------|---------|
| `Ctrl++` | Increase font size | Better readability |
| `Ctrl+-` | Decrease font size | Adjust preference |
| `Ctrl+0` | Reset font size | Default view |
| `Ctrl+Shift+D` | Toggle dark mode | Theme preference |

---

## Conflict Resolution

### Terminal Emulator Conflicts

Some shortcuts may conflict with terminal emulators:

| Shortcut | Common Conflict | Euxis Alternative |
|----------|----------------|-------------------|
| `Ctrl+C` | SIGINT (cancel) | Use sparingly, prefer Esc |
| `Ctrl+Z` | SIGTSTP (suspend) | Not used in Euxis |
| `Ctrl+D` | EOF (logout) | Avoid in input contexts |
| `Ctrl+S` | Terminal flow control | Use only in specific contexts |
| `Ctrl+Q` | Terminal flow control | Not used in Euxis |

### Browser Conflicts (Future Web UI)

| Shortcut | Browser Function | Euxis Strategy |
|----------|-----------------|----------------|
| `Ctrl+K` | Address bar focus | Override with preventDefault |
| `Ctrl+L` | Address bar focus | Not used |
| `Ctrl+T` | New tab | Use `Ctrl+Shift+T` instead |
| `Ctrl+W` | Close tab | Not used |
| `F5` | Refresh | Allow default behavior |

### OS Conflicts

| Shortcut | OS Function | Euxis Strategy |
|----------|-------------|----------------|
| `Alt+Tab` | Window switching | Never override |
| `Cmd/Win+Space` | System search | Never override |
| `Ctrl+Alt+Del` | System menu | Never override |

---

## Customization (Future)

Planned shortcut customization system:

### Configuration Format
```json
{
  "shortcuts": {
    "global": {
      "command-palette": ["Ctrl+K", "Ctrl+Shift+P"],
      "help": ["Ctrl+/", "F1"],
      "close": ["Esc"]
    },
    "context": {
      "euxis-ui": {
        "deploy-agent": ["1", "Alt+D"],
        "switch-provider": ["2", "Alt+P"]
      }
    }
  }
}
```

### Override Rules
1. User preferences override defaults
2. Context-specific shortcuts take precedence
3. OS-level shortcuts are never overridden
4. Accessibility shortcuts are protected
5. Conflicts trigger warnings with suggestions

---

## Implementation Notes

### Current Status
- ✅ **Basic navigation**: Arrow keys, Enter, Tab work in current euxis-ui
- 🔶 **Command palette**: Specified but not implemented
- 🔶 **Global shortcuts**: Planned for future releases
- 🔶 **Accessibility**: Basic terminal support only

### Technical Requirements
- **Terminal detection**: Capability sensing for advanced features
- **Input handling**: Clean separation of system vs application shortcuts
- **State management**: Focus tracking and restoration
- **Documentation**: Auto-generated help from shortcut registry

### Testing Checklist
- [ ] All shortcuts work without mouse
- [ ] No conflicts with critical system shortcuts
- [ ] Accessible alternatives for complex gestures
- [ ] Consistent behavior across contexts
- [ ] Clear visual feedback for all actions
- [ ] Help system shows current shortcuts
- [ ] Customization preserves core functionality

---

*This reference reflects current implementation and planned enhancements. Shortcuts marked as "Planned" are specified by the UI agent fleet but not yet implemented.*

---

Designed by Sebastien Rousseau — Engineered with Euxis