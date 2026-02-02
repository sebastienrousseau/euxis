# Euxis UI & Interaction Guide

**Enterprise Unified eXecution Intelligence System**

Version 0.0.7

---

## Overview

Euxis implements a dual-interface model: a terminal-native CLI for power users and a web-based interface for broader accessibility. Both interfaces share a unified interaction philosophy centered on keyboard-first design, progressive disclosure, and accessibility.

## UI Agent Architecture

The Euxis Fleet includes four specialized UI agents working in coordination:

### Agent Responsibilities

| Agent | Primary Role | Key Outputs |
|-------|-------------|-------------|
| `interaction-and-input-specialist` | Keyboard navigation model, focus management, shortcut registry | Focus order maps, command palette design, shortcut documentation |
| `cli-ui-artisan` | Terminal UI layout and TUI components | Panel layouts, keyboard patterns, terminal adaptation |
| `web-ui-architect` | Design system and component contracts | Token systems, component APIs, responsive behavior |
| `theming-and-motion-engineer` | Color tokens, Light/Dark modes, motion rules | Theme definitions, contrast matrices, animation specifications |

### Coordination Patterns

- **Design System Flow**: `web-ui-architect` defines tokens → `theming-and-motion-engineer` provides values → `cli-ui-artisan` adapts for terminal
- **Interaction Flow**: `interaction-and-input-specialist` defines patterns → Both UI architects implement in their domains
- **Accessibility Flow**: `ux-sentinel` identifies issues → UI agents collaborate on solutions

---

## Keyboard Navigation Model

### Universal Principles

All Euxis interfaces follow these keyboard navigation rules:

1. **Tab Progression**: Tab moves between logical groups/regions
2. **Arrow Navigation**: Arrows move within groups (roving tabindex)
3. **Enter Activation**: Enter activates the focused element
4. **Escape Dismissal**: Escape dismisses overlays/returns to parent
5. **Spatial Logic**: Navigation follows visual layout (up/down, left/right)

---

## Keyboard Shortcuts Reference

### Global Shortcuts

| Shortcut | Action | Context | Status |
|----------|--------|---------|--------|
| `Ctrl+K` | Open command palette | Global | Planned |
| `Ctrl+Shift+P` | Open command palette (alternate) | Global | Planned |
| `Ctrl+/` | Show keyboard shortcuts help | Global | Planned |
| `F1` | Help/Documentation | Global | Planned |
| `Esc` | Close modal/dialog/palette | Overlays | Partial |

### Focus Management

| Shortcut | Action | Context | Status |
|----------|--------|---------|--------|
| `Tab` | Next focusable element | Global | Basic |
| `Shift+Tab` | Previous focusable element | Global | Basic |
| `Arrow Keys` | Navigate within groups | Lists, Menus | Basic |
| `Enter` | Activate focused element | Interactive | Basic |
| `Space` | Select/toggle focused element | Checkboxes, Buttons | Planned |

### euxis-ui (Terminal Interface)

#### Main Menu

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

#### Agent Selection

| Shortcut | Action | Notes |
|----------|--------|-------|
| `↑/↓` | Navigate agent list | Standard select navigation |
| `Enter` | Choose agent | Proceeds to task input |
| `Cancel` | Return to main menu | Select "Cancel" option |

#### Provider Selection

| Shortcut | Action | Notes |
|----------|--------|-------|
| `↑/↓` | Navigate provider list | claude, gemini, openai, ollama, opencode |
| `Enter` | Choose provider | Sets current provider |
| `Cancel` | Keep current provider | Select "Cancel" option |

### Agent-Specific Shortcuts (Planned)

| Agent | Shortcut | Action | Context |
|-------|----------|--------|---------|
| `orchestrator` | `Ctrl+D` | Create dispatch manifest | Planning mode |
| `orchestrator` | `Ctrl+M` | Switch to mesh mode | Coordination |
| `orchestrator` | `Ctrl+F` | Switch to federated mode | Coordination |
| `architect` | `Ctrl+P` | Create pattern template | Design mode |
| `architect` | `Ctrl+R` | Review architecture | Analysis mode |
| `edge-hunter` | `Ctrl+S` | Start security scan | Audit mode |
| `edge-hunter` | `Ctrl+V` | View vulnerability report | Results |

### Accessibility Shortcuts (Planned)

| Shortcut | Action | Purpose |
|----------|--------|---------|
| `Ctrl+Alt+T` | Toggle TTS mode | Voice feedback |
| `Ctrl+Alt+V` | Verbose mode | Detailed descriptions |
| `Ctrl+Alt+H` | Heading navigation | Skip to sections |
| `Ctrl++` | Increase font size | Better readability |
| `Ctrl+-` | Decrease font size | Adjust preference |
| `Ctrl+0` | Reset font size | Default view |
| `Ctrl+Shift+D` | Toggle dark mode | Theme preference |

---

## Command Palette Specification

### Requirements (per interaction-and-input-specialist)

The command palette MUST support:
- **Fuzzy Search**: Match commands by name, description, or tags
- **Shortcut Display**: Show keyboard shortcuts inline, right-aligned
- **Recent Actions**: Surface recently used commands first
- **Categorized Commands**: Group by agent, function, or context

### Categories

| Category | Description | Example Commands |
|----------|-------------|------------------|
| **Agents** | Deploy individual agents | "Run architect", "Deploy edge-hunter" |
| **Squads** | Deploy agent teams | "Deploy Vision squad", "Deploy Quality squad" |
| **Tools** | Run Euxis utilities | "Fleet health", "Run certification", "Lint checks" |
| **Navigation** | Move between views | "View logs", "Switch provider", "Change directory" |
| **Settings** | Configuration | "Change provider", "Set preferences" |

### Quick Action Patterns

| Pattern | Description | Examples |
|---------|-------------|----------|
| `> command` | Direct command execution | `> health`, `> lint`, `> certify` |
| `@ agent` | Agent deployment | `@ architect Review auth module` |
| `# squad` | Squad deployment | `# quality Audit payment service` |
| `? help` | Documentation lookup | `? keyboard shortcuts`, `? agents` |
| `! tool` | Utility execution | `! bench`, `! audit-run` |

### Implementation Status

- **CLI**: Not implemented (current euxis-ui uses basic menus)
- **Web**: Not implemented
- **Specification**: Defined by `interaction-and-input-specialist`

---

## Focus Management Rules

### Focus Traps

| Element | Trap Scope | Restore Target | Escape Behavior |
|---------|-----------|----------------|-----------------|
| Command Palette | Search input + results | Previous focus | Close + restore |
| Modal Dialogs | All interactive elements within | Modal trigger | Close + restore |
| Dropdown Menus | Menu items only | Menu trigger | Close + restore |

### Focus Indicators

All interactive elements MUST have visible focus indicators meeting:
- **Contrast**: 4.5:1 minimum ratio (WCAG 2.1 AA)
- **Visibility**: High-contrast outline or background change
- **Consistency**: Same treatment across similar elements

---

## Design Token System

### Semantic Color Tokens (Light/Dark)

| Token | Light | Dark | Purpose |
|-------|--------|------|---------|
| `color-text-primary` | `#1a1a1a` | `#f5f5f5` | Main content text |
| `color-text-secondary` | `#6b6b6b` | `#a1a1a1` | Supporting text |
| `color-surface-primary` | `#ffffff` | `#1a1a1a` | Main backgrounds |
| `color-surface-secondary` | `#f8f8f8` | `#262626` | Panel backgrounds |
| `color-accent-primary` | `#0066cc` | `#3d9eff` | Primary actions |
| `color-border-subtle` | `#e5e5e5` | `#404040` | Subtle borders |
| `color-focus-ring` | `#0066cc` | `#3d9eff` | Focus indicators |

### Typography Scale

| Token | Size | Weight | Usage |
|-------|------|--------|-------|
| `type-heading-1` | 2rem | 700 | Page titles |
| `type-heading-2` | 1.5rem | 600 | Section headers |
| `type-body` | 1rem | 400 | Body text |
| `type-body-small` | 0.875rem | 400 | Secondary info |
| `type-code` | 0.875rem | 400 | Code snippets |

### Spacing Scale

| Token | Value | Usage |
|-------|--------|--------|
| `space-xs` | 0.25rem | Icon padding |
| `space-sm` | 0.5rem | Component padding |
| `space-md` | 1rem | Section spacing |
| `space-lg` | 1.5rem | Panel margins |
| `space-xl` | 2rem | Page margins |

---

## Motion Rules

### Animation Principles

1. **Functional Motion**: Animations communicate state change or spatial relationships
2. **Reduced Motion**: Respect `prefers-reduced-motion: reduce`
3. **Performance**: 60fps target, hardware acceleration when possible

### Duration Scale

| Token | Value | Usage |
|-------|--------|--------|
| `duration-instant` | 0ms | Reduced motion fallback |
| `duration-fast` | 150ms | Micro-interactions |
| `duration-normal` | 300ms | Standard transitions |
| `duration-slow` | 500ms | Complex state changes |

### Easing Curves

| Curve | CSS Value | Usage |
|-------|-----------|--------|
| `ease-out` | `cubic-bezier(0.25, 0.46, 0.45, 0.94)` | Enters, reveals |
| `ease-in` | `cubic-bezier(0.55, 0.06, 0.68, 0.19)` | Exits, dismissals |
| `ease-in-out` | `cubic-bezier(0.42, 0, 0.58, 1)` | Transforms |

---

## Shortcut Conflict Resolution

### Terminal Emulator Conflicts

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

## Shortcut Customization (Planned)

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

## Implementation Status & Roadmap

### Current Status
- **Basic navigation**: Arrow keys, Enter, Tab work in current euxis-ui
- **Command palette**: Specified but not implemented
- **Global shortcuts**: Planned for future releases
- **Accessibility**: Basic terminal support only

### euxis-ui Limitations

- Menu-driven navigation (implemented)
- Agent selection and deployment (implemented)
- Provider switching (implemented)
- Command palette (not implemented)
- Advanced keyboard patterns (not implemented)
- Focus management (not implemented)

### Roadmap

**Phase 1 — Core Patterns**: Global keyboard shortcuts, command palette with fuzzy search, focus management, shortcut registry format.

**Phase 2 — CLI Enhancement**: Upgrade euxis-ui with advanced keyboard patterns, add command palette, implement focus traps, accessibility features.

**Phase 3 — Web Interface**: Build web UI with design token system, responsive breakpoints, dark mode, cross-platform keyboard compatibility.

**Phase 4 — Integration**: Unified shortcut registry, cross-platform preference sync, advanced accessibility, performance optimization.

---

## Testing Checklists

### Keyboard Navigation
- [ ] All interactive elements reachable via keyboard
- [ ] Focus indicators visible and high-contrast
- [ ] Tab order follows logical sequence
- [ ] Arrow keys work within groups
- [ ] Escape dismisses overlays correctly
- [ ] All shortcuts work without mouse
- [ ] No conflicts with critical system shortcuts
- [ ] Consistent behavior across contexts

### Accessibility
- [ ] Screen reader compatibility
- [ ] High contrast mode support
- [ ] Reduced motion preference respected
- [ ] WCAG 2.1 AA compliance
- [ ] Accessible alternatives for complex gestures

### Cross-Platform
- [ ] Terminal compatibility (various emulators)
- [ ] OS keyboard shortcut conflicts
- [ ] Browser compatibility (web interface)
- [ ] Mobile touch equivalent patterns

---

Designed by Sebastien Rousseau — Engineered with Euxis
