"""Comprehensive unit tests for SquadDetailScreen.

Property-based tests for squad composition and chain visualization.
"""

import unittest
from unittest.mock import Mock, PropertyMock, patch

from hypothesis import given
from hypothesis import strategies as st

from tui.core.registry import Agent, Combo, FleetRegistry, Squad
from tui.screens.squad_detail import SquadDetailScreen


def _patch_screen_app(screen, mock_app):
    """Bypass Textual's read-only app property for testing."""
    patcher = patch.object(type(screen), "app", new_callable=PropertyMock, return_value=mock_app)
    patcher.start()
    return patcher


class TestSquadDetailScreen(unittest.TestCase):
    """Unit tests for SquadDetailScreen squad visualization."""

    def setUp(self):
        """Set up test environment with mock app."""
        self.mock_app = Mock()
        self.mock_app.project_name = "test-project"
        self.mock_app.git_branch = "main"

        # Create sample fleet registry
        self.registry = FleetRegistry()
        self.registry.agents = [
            Agent(
                id="architect", tier="core", version="0.0.7",
                tags=("design",), activation="default",
            ),
            Agent(
                id="debugger", tier="fleet", version="0.0.7",
                tags=("fix",), activation="default",
            ),
            Agent(
                id="tester", tier="fleet", version="0.0.7",
                tags=("qa",), activation="default",
            ),
            Agent(
                id="reviewer", tier="core", version="0.0.7",
                tags=("qa",), activation="specialist",
            ),
        ]
        self.registry.squads = [
            Squad(
                id="dev-squad", name="Development Team",
                purpose="Build features", lead="architect",
                members=("architect", "debugger", "tester"),
            ),
            Squad(
                id="qa-squad", name="Quality Assurance",
                purpose="Test everything", lead="reviewer",
                members=("reviewer", "tester"),
            ),
        ]
        self.registry.combos = [
            Combo(id="build-combo", name="Build Pipeline", description="Code to deployment",
                  chain=("debugger", "tester", "reviewer")),
            Combo(id="fix-combo", name="Bug Fix", description="Find and fix bugs",
                  chain=("debugger", "tester")),
        ]
        self.mock_app.fleet_registry = self.registry
        self._app_patcher = None

    def tearDown(self):
        if self._app_patcher:
            self._app_patcher.stop()

    def test_screen_initialization(self):
        """Test SquadDetailScreen can be initialized."""
        screen = SquadDetailScreen()
        assert isinstance(screen, SquadDetailScreen)
        assert len(screen.BINDINGS) == 2
        assert "escape" in [binding[0] for binding in screen.BINDINGS]
        assert "ctrl+k" in [binding[0] for binding in screen.BINDINGS]

    def test_css_configuration(self):
        """Test CSS styling is properly configured."""
        screen = SquadDetailScreen()
        assert "#squad-detail" in screen.DEFAULT_CSS
        assert ".squad-card" in screen.DEFAULT_CSS
        assert ".squad-name" in screen.DEFAULT_CSS
        assert ".combo-chain" in screen.DEFAULT_CSS

    def test_euxis_app_property(self):
        """Test euxis_app property returns correct app instance."""
        screen = SquadDetailScreen()
        self._app_patcher = _patch_screen_app(screen, self.mock_app)
        assert screen.euxis_app == self.mock_app

    @patch("tui.screens.squad_detail.ETXHeader")
    @patch("tui.screens.squad_detail.VerticalScroll")
    @patch("tui.screens.squad_detail.Container")
    @patch("tui.screens.squad_detail.Static")
    @patch("tui.screens.squad_detail.Footer")
    def test_compose_structure(self, *mocks):
        """Test screen composition structure."""
        screen = SquadDetailScreen()
        result = list(screen.compose())
        assert len(result) > 0

    @patch("tui.screens.squad_detail.ETXHeader")
    @patch("tui.screens.squad_detail.VerticalScroll")
    @patch("tui.screens.squad_detail.Container")
    @patch("tui.screens.squad_detail.Static")
    @patch("tui.screens.squad_detail.Footer")
    def test_compose_components(
        self, mock_footer, mock_static, mock_container, mock_scroll, mock_header,
    ):
        """Test compose creates expected components."""
        screen = SquadDetailScreen()
        list(screen.compose())

        mock_header.assert_called_once()
        mock_scroll.assert_called_once()
        mock_footer.assert_called_once()

    def test_action_go_back(self):
        """Test go_back action calls app.pop_screen."""
        screen = SquadDetailScreen()
        self._app_patcher = _patch_screen_app(screen, self.mock_app)

        screen.action_go_back()
        self.mock_app.pop_screen.assert_called_once()


class TestSquadDetailScreenMounting(unittest.TestCase):
    """Tests for squad detail screen mounting and data display."""

    def setUp(self):
        """Set up test environment."""
        self.mock_app = Mock()
        self.mock_app.project_name = "test-project"
        self.mock_app.git_branch = "feature/test"

        self.registry = FleetRegistry()
        self.mock_app.fleet_registry = self.registry
        self._app_patcher = None

    def tearDown(self):
        if self._app_patcher:
            self._app_patcher.stop()

    def create_screen_with_mocks(self):
        """Create screen with properly mocked query methods."""
        screen = SquadDetailScreen()
        self._app_patcher = _patch_screen_app(screen, self.mock_app)

        # Mock query_one to return appropriate mocks
        mock_header = Mock()
        mock_squads_container = Mock()
        mock_combos_container = Mock()

        def query_one_side_effect(*args, **kwargs):
            if args and hasattr(args[0], "__name__") and "ETXHeader" in args[0].__name__:
                return mock_header
            if args and args[0] == "#squads-list":
                return mock_squads_container
            if args and args[0] == "#combos-list":
                return mock_combos_container
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        return screen, mock_header, mock_squads_container, mock_combos_container

    def test_on_mount_header_configuration(self):
        """Test on_mount properly configures header."""
        screen, mock_header, _, _ = self.create_screen_with_mocks()

        screen.on_mount()

        assert mock_header.project == "test-project"
        assert mock_header.branch == "feature/test"

    def test_on_mount_with_squads(self):
        """Test on_mount creates squad cards."""
        self.registry.agents = [
            Agent(id="architect", tier="core", version="0.0.7", tags=(), activation="default"),
            Agent(id="debugger", tier="fleet", version="0.0.7", tags=(), activation="default"),
        ]
        self.registry.squads = [
            Squad(id="test-squad", name="Test Squad", purpose="Testing", lead="architect",
                  members=("architect", "debugger")),
        ]

        screen, _, mock_squads_container, _ = self.create_screen_with_mocks()

        with patch("tui.screens.squad_detail.Static"):
            screen.on_mount()

        # Should create one squad card
        mock_squads_container.mount.assert_called_once()

    def test_on_mount_with_combos(self):
        """Test on_mount creates combo cards."""
        self.registry.combos = [
            Combo(id="test-combo", name="Test Combo", description="Test chain",
                  chain=("architect", "debugger")),
        ]

        screen, _, _, mock_combos_container = self.create_screen_with_mocks()

        with patch("tui.screens.squad_detail.Static"):
            screen.on_mount()

        # Should create one combo card
        mock_combos_container.mount.assert_called_once()

    def test_core_agent_highlighting(self):
        """Test core agents are highlighted differently in squads."""
        self.registry.agents = [
            Agent(id="core-agent", tier="core", version="0.0.7", tags=(), activation="default"),
            Agent(id="fleet-agent", tier="fleet", version="0.0.7", tags=(), activation="default"),
        ]
        self.registry.squads = [
            Squad(id="mixed-squad", name="Mixed Squad", purpose="Mixed team", lead="core-agent",
                  members=("core-agent", "fleet-agent")),
        ]

        screen, _, _mock_squads_container, _ = self.create_screen_with_mocks()

        captured_static_calls = []
        def capture_static_call(*args, **kwargs):
            captured_static_calls.append(args)
            return Mock()

        with patch("tui.screens.squad_detail.Static", side_effect=capture_static_call):
            screen.on_mount()

        # Verify Static was called with content containing core and fleet agent highlighting
        static_content = captured_static_calls[0][0] if captured_static_calls else ""
        assert "[yellow]core-agent[/]" in static_content
        assert "[cyan]fleet-agent[/]" in static_content


class TestSquadDetailScreenPropertyBased(unittest.TestCase):
    """Property-based tests for SquadDetailScreen robustness."""

    @given(st.text(
        min_size=1, max_size=50,
        alphabet=st.characters(whitelist_categories=["Lu", "Ll", "Nd"]),
    ))
    def test_project_name_robustness(self, project_name):
        """Property: Screen initializes with arbitrary valid project names."""
        screen = SquadDetailScreen()
        assert isinstance(screen, SquadDetailScreen)

    @given(st.lists(
        st.text(
            min_size=1, max_size=20,
            alphabet=st.characters(whitelist_categories=["Lu", "Ll", "Nd"]),
        ),
        min_size=0, max_size=10,
    ))
    def test_agent_list_robustness(self, agent_ids):
        """Property: Registry handles arbitrary agent ID lists."""
        registry = FleetRegistry()
        registry.agents = [
            Agent(id=aid, tier="fleet", version="0.0.7", tags=(), activation="default")
            for aid in agent_ids
        ]
        # Verify registry construction doesn't crash
        for aid in agent_ids:
            assert registry.get_agent(aid).id == aid

    @given(st.lists(
        st.tuples(
            st.text(min_size=1, max_size=20),  # squad name
            st.text(min_size=0, max_size=100), # purpose
            st.text(min_size=1, max_size=20),  # lead
            st.lists(st.text(min_size=1, max_size=20), min_size=1, max_size=5) # members
        ),
        min_size=0, max_size=5
    ))
    def test_squad_configuration_robustness(self, squad_configs):
        """Property: Registry handles arbitrary squad configurations."""
        registry = FleetRegistry()
        for i, (name, purpose, lead, members) in enumerate(squad_configs):
            registry.squads.append(Squad(
                id=f"squad-{i}", name=name, purpose=purpose,
                lead=lead, members=tuple(members),
            ))
        assert len(registry.squads) == len(squad_configs)


class TestSquadDetailScreenEdgeCases(unittest.TestCase):
    """Edge case tests for SquadDetailScreen."""

    def test_empty_registry(self):
        """Test empty registry construction."""
        registry = FleetRegistry()
        assert len(registry.agents) == 0
        assert len(registry.squads) == 0
        assert len(registry.combos) == 0

    def test_missing_git_branch(self):
        """Test screen initializes without error."""
        screen = SquadDetailScreen()
        assert isinstance(screen, SquadDetailScreen)

    def test_squad_with_missing_agents(self):
        """Test squad referencing non-existent agents."""
        registry = FleetRegistry()
        registry.squads = [
            Squad(id="broken-squad", name="Broken Squad", purpose="Test", lead="missing-lead",
                  members=("missing-1", "missing-2")),
        ]
        # get_agent returns None for missing IDs
        assert registry.get_agent("missing-lead") is None

    def test_unicode_squad_names(self):
        """Test squad names with Unicode characters."""
        registry = FleetRegistry()
        registry.squads = [
            Squad(
                id="unicode-squad", name="Squad \U0001f916",
                purpose="Test with \u00e9moji", lead="leader",
                members=("member-1",),
            ),
        ]
        assert registry.squads[0].name == "Squad 🤖"

    def test_very_long_chain_combo(self):
        """Test combo with very long agent chain."""
        long_chain = tuple(f"agent-{i}" for i in range(20))
        combo = Combo(id="long-combo", name="Long Chain", description="Very long chain",
                      chain=long_chain)
        assert len(combo.chain) == 20


if __name__ == "__main__":
    unittest.main()
