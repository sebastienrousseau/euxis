#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Entry point for python -m tui.

Usage:
    python -m tui           # Normal TUI mode
    python -m tui --audit   # Run performance audit
    python -m tui --help    # Show help
"""

import sys


def run_audit() -> None:
    """Run TUI performance audit and cross-platform compatibility checks."""
    import os
    import shutil
    import time

    print("╔═══════════════════════════════════════════════════════════════╗")
    print("║           Euxis TUI Performance Audit v0.0.2                  ║")
    print("╚═══════════════════════════════════════════════════════════════╝")
    print()

    # 1. Environment Detection
    print("─── Environment Detection ───")
    term = os.environ.get("TERM", "unknown")
    colorterm = os.environ.get("COLORTERM", "")
    term_program = os.environ.get("TERM_PROGRAM", "")
    wsl = "WSL" if "microsoft" in os.uname().release.lower() else ""

    print(f"  TERM:         {term}")
    print(f"  COLORTERM:    {colorterm or '(not set)'}")
    print(f"  TERM_PROGRAM: {term_program or '(not set)'}")
    print(f"  Platform:     {sys.platform} {wsl}")
    print(f"  Terminal:     {shutil.get_terminal_size()}")
    print()

    # 2. Color Support
    print("─── Color Support ───")
    colors_256 = "256color" in term or colorterm in ("truecolor", "24bit")
    colors_true = colorterm in ("truecolor", "24bit")
    print(f"  256 colors:   {'✓ Yes' if colors_256 else '✗ No (16-color fallback)'}")
    print(f"  True color:   {'✓ Yes' if colors_true else '✗ No'}")
    print()

    # 3. Unicode Support
    print("─── Unicode Support ───")
    test_chars = [
        ("Box drawing", "─│┌┐└┘├┤┬┴┼"),
        ("Progress", "█▓▒░"),
        ("Braille", "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"),
        ("Status icons", "●◐◆✓✗⚠"),
        ("Emoji", "🌡⏸🔥"),
    ]
    for name, chars in test_chars:
        print(f"  {name:14} {chars}")
    print("  (Verify above characters render correctly)")
    print()

    # 4. Framework Import Test
    print("─── Framework Tests ───")
    try:
        start = time.perf_counter()
        from textual.app import App  # noqa: F401
        from rich.text import Text  # noqa: F401
        import_time = (time.perf_counter() - start) * 1000
        print(f"  Textual/Rich: ✓ Imported ({import_time:.1f}ms)")
    except ImportError as e:
        print(f"  Textual/Rich: ✗ {e}")

    try:
        from tui.app import EuxisApp  # noqa: F401
        print("  EuxisApp:     ✓ Loaded")
    except ImportError as e:
        print(f"  EuxisApp:     ✗ {e}")

    print()

    # 5. Rendering Benchmark
    print("─── Rendering Benchmark ───")
    try:
        from io import StringIO
        from rich.console import Console
        from rich.text import Text as RichText

        # Use StringIO to capture output (no actual terminal writes)
        buffer = StringIO()
        console = Console(file=buffer, force_terminal=True, width=80)

        # Benchmark: Render 1000 lines to buffer
        lines = [RichText(f"Line {i}: " + "x" * 60) for i in range(1000)]
        start = time.perf_counter()
        for line in lines:
            console.print(line)
        render_time = (time.perf_counter() - start) * 1000
        print(f"  1000 lines:   {render_time:.1f}ms ({1000 / (render_time / 1000):.0f} lines/sec)")

        # Benchmark: Widget update simulation (Text object creation)
        start = time.perf_counter()
        for _ in range(100):
            _ = RichText("Status: Running...", style="bold cyan")
        update_time = (time.perf_counter() - start) * 1000
        print(f"  100 updates:  {update_time:.2f}ms ({100 / (update_time / 1000):.0f} updates/sec)")

        # Memory efficiency check
        buffer_size = len(buffer.getvalue())
        print(f"  Buffer size:  {buffer_size / 1024:.1f}KB for 1000 lines")

    except Exception as e:
        print(f"  Benchmark:    ✗ {e}")

    print()

    # 6. Virtual Scrolling Test
    print("─── Virtual Scrolling Test ───")
    try:
        from textual.widgets import RichLog

        # RichLog handles virtual scrolling internally
        print("  RichLog:      ✓ Available (virtual scrolling enabled)")
        print("  Max buffer:   ~10,000 lines (framework default)")
    except ImportError:
        print("  RichLog:      ✗ Not available")

    print()

    # 7. Signal Handling
    print("─── Signal Handling ───")
    import signal
    handlers = {
        "SIGINT": signal.getsignal(signal.SIGINT),
        "SIGTERM": signal.getsignal(signal.SIGTERM),
    }
    for sig, handler in handlers.items():
        handler_name = getattr(handler, "__name__", str(handler))
        print(f"  {sig:10}  {handler_name}")
    print("  (Textual will install proper handlers on startup)")

    print()

    # 8. Cross-Platform Notes
    print("─── Cross-Platform Notes ───")
    if sys.platform == "darwin":
        print("  macOS:        Use iTerm2 or Ghostty for best results")
        print("                Terminal.app has limited Unicode/emoji support")
    elif sys.platform == "linux":
        if wsl:
            print("  WSL:          Use Windows Terminal for mouse support")
            print("                Legacy console may have issues")
        else:
            print("  Linux:        Most terminals work well")
            print("                Verify TERM=xterm-256color or similar")
    elif sys.platform == "win32":  # pragma: no cover
        print("  Windows:      Use Windows Terminal (not cmd.exe)")
        print("                Enable virtual terminal processing")

    print()
    print("═══════════════════════════════════════════════════════════════")
    print("  Audit complete. Run 'python -m tui' to start the TUI.")
    print("═══════════════════════════════════════════════════════════════")


def show_help() -> None:
    """Show help message."""
    print("Euxis TUI — Terminal User Interface for Euxis Agent Framework")
    print()
    print("Usage:")
    print("  python -m tui           Start the TUI")
    print("  python -m tui --audit   Run performance audit")
    print("  python -m tui --help    Show this help")
    print()
    print("Keyboard Shortcuts:")
    print("  Ctrl+K    Command palette")
    print("  F3        Toggle theme")
    print("  Ctrl+M    Fleet monitor")
    print("  F4        Settings")
    print("  F6        Logs")
    print("  F1        Help screen")
    print("  /         Filter/search")
    print("  Ctrl+Q    Quit")


def main() -> None:
    """Main entry point with argument handling."""
    if len(sys.argv) > 1:
        arg = sys.argv[1].lower()
        if arg in ("--audit", "-a", "audit"):
            run_audit()
            return
        if arg in ("--help", "-h", "help"):
            show_help()
            return
        print(f"Unknown argument: {sys.argv[1]}")
        print("Use --help for usage information.")
        sys.exit(1)

    # Normal TUI mode
    from tui.app import EuxisApp
    app = EuxisApp()
    app.run()


if __name__ == "__main__":
    main()
