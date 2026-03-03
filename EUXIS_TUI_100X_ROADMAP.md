# Euxis TUI 2026: The "100% Better" Implementation Plan

## 1. 2026 AI TUI & UX/UI Trends Analysis

In 2026, the AI Terminal User Interface (TUI) has shifted from being a simple wrapper around LLM APIs into a full-fledged **Agentic Development Environment (ADE)**. Users no longer want to just chat; they expect the terminal to be a context-aware, multimodal, and hyper-fast orchestrator.

### The 2026 UI/UX Standard:
1. **Glassmorphism & Floating Elements in Terminal:** TUIs now support overlays, floating command palettes (Command+K), and dropdowns right in the terminal, breaking out of strict linear text flow.
2. **Block-Based Interaction:** Similar to Notion or Jupyter, interactions are "blocks". A block contains the user's prompt, the AI's reasoning (thought bubbles), the executed commands, and the final output. These blocks can be collapsed, copied, or re-run.
3. **AAA Accessibility & Contrast:** Strict adherence to WCAG AAA standards. Colors aren't just "pretty"; they are semantic. Syntax highlighting adjusts dynamically based on the terminal's background (light/dark mode auto-detection).
4. **Sub-5ms Latency (The "Instant" Standard):** Input lag is the enemy. While network requests take time, the UI itself must react to keystrokes and commands in under 5ms. Streaming responses are non-negotiable.
5. **Invisible Context:** The AI automatically knows what file you are looking at, what git branch you are on, and what error just appeared in the terminal buffer.

### Competitor Weaknesses:
- **Charmbracelet Crush:** Gorgeous, but written in Go. Under heavy load or large context, GC pauses and Bubbletea overhead can cause input lag >15ms.
- **Claude Code:** Powerful agentic capabilities, but visually austere. It feels like a 2023 CLI tool. No floating windows or rich block interactions.
- **Gemini CLI:** Fast, but lacks deep multi-agent coordination. It's a 1-to-1 chat tool, not a swarm orchestrator.

## 2. Euxis TUI: The Current State vs. The Goal
Our current `euxis tui` has a solid foundation: it uses C++23 (meaning it is theoretically much faster than Go/Node competitors), it has basic raw terminal mode (termios), a background render loop, and the Catppuccin color palette. 

**However, to be 100% better, we need to move from a "Terminal Chatbot" to a "Hardware-Accelerated Agentic Dashboard."**

---

## 3. The Implementation Plan (To Beat Them All)

### Phase 1: The "Sub-5ms" Rendering Engine (C++ Hardware Dominance)
To achieve <=5ms response times, we must bypass standard `std::cout` buffering and implement a true double-buffered TUI engine.
- **Action 1:** Replace `std::cout` with raw `write(STDOUT_FILENO, ...)` of VT100/ANSI byte streams. 
- **Action 2:** Implement a **Double-Buffer System**. Create two 2D arrays representing the terminal grid (width x height). Render the UI to the "back buffer", calculate the exact diff against the "front buffer", and emit only the necessary ANSI cursor movement and color codes. This guarantees zero flickering and sub-1ms render times.
- **Action 3:** Offload syntax highlighting to a background thread. When streaming AI output, the main thread renders raw text immediately, while a background thread computes syntax colors and updates the buffer seamlessly.

### Phase 2: "Crush-Killer" Aesthetics & AAA Compliancy
We will implement a gorgeous, dynamic layout that makes Claude Code look obsolete.
- **Action 1:** Implement **Floating Command Palette**. Hitting `Ctrl+K` opens a floating ASCII box over the chat, allowing instant agent switching, context loading, and tool execution with fuzzy search.
- **Action 2:** AAA Color Contrast Enforcement. Implement a function that detects terminal background color (via OSC 11 response) and dynamically shifts the Catppuccin palette to guarantee a minimum 7:1 contrast ratio.
- **Action 3:** Rich Markdown & Code Blocks. Code blocks get distinct background colors, rounded Unicode corners (`╭`, `╰`), and a dedicated "Copy" hotkey.

### Phase 4: Agentic Swarm Visualization (The "Claude Code" Killer)
Euxis supports multiple agents. The TUI must visualize them working together.
- **Action 1:** Implement **Collapsible Thought Bubbles**. When `@architect` delegates to `@code-agent`, the TUI shows a tree structure. 
  ```text
  ╭── architect 
  │ ├─ delegating to code-agent... (done)
  │ ╰─ analyzing results... (done)
  ```
- **Action 2:** Live Context Sidebar. Use a portion of the screen (e.g., right 30 columns) as a permanent "Context Inspector." It automatically lists the files the AI is currently reading, the active git branch, and memory usage.

## 4. Execution Roadmap

**PR 1: The Engine Upgrade (Performance & Architecture)**
- Implement the Double-Buffered Grid struct.
- Shift terminal I/O to raw `write()`.
- Measure latency. Goal: Input to render < 2ms.

**PR 2: The Block UI & Aesthetics (Beauty & AAA)**
- Implement the Catppuccin AAA-compliant dynamic palette.
- Render interactions as distinct blocks with rounded corners.
- Add live syntax highlighting for streamed text.

**PR 3: The Command Palette & Overlays (UX Innovation)**
- Build the `Ctrl+K` floating overlay engine.
- Integrate fuzzy-search for commands and agents.

**PR 4: Swarm Visualization (The Moat)**
- Hook up the `RegistryClient` to the UI to show active sub-agents.
- Add the real-time context sidebar.