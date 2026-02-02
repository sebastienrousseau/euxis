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

### Global Shortcuts

| Shortcut | Context | Action | Agent Responsible |
|----------|---------|--------|-------------------|
| `Ctrl+K` | Global | Open command palette | `interaction-and-input-specialist` |
| `Ctrl+Shift+P` | Global | Open command palette | `interaction-and-input-specialist` |
| `Ctrl+/` | Global | Show keyboard shortcuts | `interaction-and-input-specialist` |
| `Esc` | Modal/Dialog | Close and restore focus | `interaction-and-input-specialist` |
| `Tab` | Global | Next focusable region | `interaction-and-input-specialist` |
| `Shift+Tab` | Global | Previous focusable region | `interaction-and-input-specialist` |

### CLI-Specific (euxis-ui)

| Key | Context | Action |
|-----|---------|--------|
| `1-5` | Main menu | Direct menu selection |
| `↑/↓` | Lists | Navigate options |
| `Enter` | Selection | Activate choice |
| `Ctrl+C` | Any | Cancel/return to menu |

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

## Current Implementation Gaps

### euxis-ui Limitations

The current `euxis-ui` TUI implements only basic functionality:
- ✅ Menu-driven navigation
- ✅ Agent selection and deployment
- ✅ Provider switching
- ❌ Command palette
- ❌ Keyboard shortcuts beyond arrow/enter
- ❌ Focus management
- ❌ Advanced keyboard patterns

### Recommended Improvements

1. **Command Palette Integration**: Add `Ctrl+K` command search
2. **Shortcut Documentation**: Display available shortcuts in help mode
3. **Focus Management**: Implement proper focus traps and restoration
4. **Roving Tabindex**: Use arrow keys for menu navigation
5. **Accessibility**: Add ARIA labels and screen reader support

---

## Implementation Roadmap

### Phase 1: Core Patterns
- [ ] Implement global keyboard shortcuts
- [ ] Add command palette with fuzzy search
- [ ] Establish focus management rules
- [ ] Define shortcut registry format

### Phase 2: CLI Enhancement
- [ ] Upgrade euxis-ui with advanced keyboard patterns
- [ ] Add command palette to terminal interface
- [ ] Implement proper focus traps
- [ ] Add accessibility features

### Phase 3: Web Interface
- [ ] Build web UI with design token system
- [ ] Implement responsive breakpoints
- [ ] Add dark mode with proper contrast
- [ ] Test cross-platform keyboard compatibility

### Phase 4: Integration
- [ ] Unified shortcut registry across interfaces
- [ ] Cross-platform preference sync
- [ ] Advanced accessibility features
- [ ] Performance optimization

---

## Testing & Validation

### Keyboard Navigation Tests
- [ ] All interactive elements reachable via keyboard
- [ ] Focus indicators visible and high-contrast
- [ ] Tab order follows logical sequence
- [ ] Arrow keys work within groups
- [ ] Escape dismisses overlays correctly

### Accessibility Tests
- [ ] Screen reader compatibility
- [ ] High contrast mode support
- [ ] Reduced motion preference respected
- [ ] WCAG 2.1 AA compliance

### Cross-Platform Tests
- [ ] Terminal compatibility (various emulators)
- [ ] OS keyboard shortcut conflicts
- [ ] Browser compatibility (web interface)
- [ ] Mobile touch equivalent patterns

---

*This guide reflects the coordinated specifications from the UI agent fleet and provides implementation guidance for consistent interaction patterns across all Euxis interfaces.*

---

Designed by Sebastien Rousseau — Engineered with Euxis