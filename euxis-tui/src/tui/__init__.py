# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

# ETX: Euxis Terminal Experience
"""Euxis Terminal Experience - Modern TUI for the Euxis Agent Fleet."""

__version__ = "0.1.0"


def main():
    """Entry point for euxis-tui command."""
    from tui.app import EuxisApp
    app = EuxisApp()
    app.run()
