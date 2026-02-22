# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

# ETX: Euxis Terminal Experience
"""Euxis Terminal Experience - Modern TUI for the Euxis Agent Fleet."""

__version__ = "v0.0.2"


def main():
    """Entry point for euxis-tui command."""
    from tui.app import EuxisApp
    app = EuxisApp()
    app.run()
