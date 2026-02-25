# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

from textual.app import ComposeResult
from textual.screen import Screen
from textual.widgets import Header, Footer, Tree, Static
from textual.containers import Vertical, Horizontal
import sys
from pathlib import Path

# Add cli to path so we can import omnigraph
repo_root = Path(__file__).resolve().parents[4]
cli_src = repo_root / "euxis-cli" / "src"
if str(cli_src) not in sys.path:
    sys.path.insert(0, str(cli_src))

from cli.omnigraph.mapper import WorkspaceMapper
from cli.omnigraph.budgeter import TokenBudgeter

class OmniGraphScreen(Screen):
    """Interactive Dependency Tree and Context Manager."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("r", "refresh", "Refresh Graph")
    ]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.workspace_root = Path("~/.euxis").expanduser()

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Horizontal():
            yield Tree("Workspace (OmniGraph)", id="graph-tree")
            with Vertical(id="context-panel"):
                yield Static("Token Budget: Pending...", id="token-budget")
                yield Static("Select a node to inspect relationships.", id="node-details")
        yield Footer()

    async def on_mount(self) -> None:
        await self.load_graph()

    async def load_graph(self) -> None:
        tree = self.query_one("#graph-tree", Tree)
        tree.root.collapse_all()
        tree.root.children.clear()

        mapper = WorkspaceMapper(str(self.workspace_root))
        graph = await mapper.scan_workspace()
        
        budgeter = TokenBudgeter()
        cost = budgeter.estimate_tokens(str(graph))
        self.query_one("#token-budget", Static).update(f"Token Budget: ~{cost} tokens")

        for node, data in graph.get("nodes", {}).items():
            tree.root.add(f"{node} ({data.get('type')})")

    async def action_refresh(self) -> None:
        await self.load_graph()

    def action_go_back(self) -> None:
        self.app.pop_screen()
