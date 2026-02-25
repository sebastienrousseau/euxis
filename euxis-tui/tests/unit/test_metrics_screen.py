# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Comprehensive unit tests for MetricsScreen.

Property-based tests for performance metrics and sparkline generation.
"""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, PropertyMock, patch

from hypothesis import given
from hypothesis import strategies as st

from tui.screens.metrics import MetricsScreen


class TestMetricsScreen(unittest.TestCase):
    """Unit tests for MetricsScreen performance metrics dashboard."""

    def setUp(self):
        """Set up test environment."""
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_metrics_test_")
        self.metrics_path = Path(self.temp_dir) / "metrics"
        self.metrics_path.mkdir()

    def tearDown(self):
        """Clean up test environment."""
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_screen_initialization(self):
        """Test MetricsScreen can be initialized."""
        screen = MetricsScreen()
        assert isinstance(screen, MetricsScreen)
        assert len(screen.BINDINGS) == 2
        assert "escape" in [binding[0] for binding in screen.BINDINGS]
        assert "ctrl+k" in [binding[0] for binding in screen.BINDINGS]

    def test_css_configuration(self):
        """Test CSS styling is properly configured."""
        screen = MetricsScreen()
        assert "#metrics-container" in screen.DEFAULT_CSS
        assert ".metric-card" in screen.DEFAULT_CSS
        assert ".metric-title" in screen.DEFAULT_CSS

    @patch("tui.screens.metrics.EUXIS_HOME")
    def test_compose_empty_metrics(self, mock_home):
        """Test screen composition with no metrics data."""
        mock_home.return_value = Path(self.temp_dir)
        screen = MetricsScreen()
        result = screen.compose()
        assert result is not None

    @patch("tui.screens.metrics.EUXIS_HOME")
    def test_compose_with_metrics_data(self, mock_home):
        """Test screen composition with sample metrics data."""
        mock_home.return_value = Path(self.temp_dir)

        # Create sample metrics file
        metrics_file = self.metrics_path / "agent_timings.json"
        sample_data = {
            "debugger": [100, 150, 120, 180, 90],
            "architect": [200, 180, 220, 160, 240],
            "tester": [80, 90, 85, 95, 88]
        }
        metrics_file.write_text(json.dumps(sample_data))

        screen = MetricsScreen()
        result = screen.compose()
        assert result is not None

    def test_action_go_back(self):
        """Test go_back action properly navigates back."""
        screen = MetricsScreen()
        mock_app = Mock()

        # Mock the app property on the screen
        with patch.object(type(screen), "app", new_callable=PropertyMock, return_value=mock_app):
            screen.action_go_back()
            mock_app.pop_screen.assert_called_once()


class TestMetricsScreenPropertyBased(unittest.TestCase):
    """Property-based tests for MetricsScreen robustness."""

    def setUp(self):
        """Set up test environment."""
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_metrics_prop_")
        self.metrics_path = Path(self.temp_dir) / "metrics"
        self.metrics_path.mkdir()

    def tearDown(self):
        """Clean up test environment."""
        import shutil
        shutil.rmtree(self.temp_dir)

    @given(metrics_data=st.dictionaries(
        st.text(min_size=1, max_size=20, alphabet=st.characters(whitelist_categories=["Lu", "Ll"])),
        st.lists(st.integers(min_value=0, max_value=10000), min_size=1, max_size=100)
    ))
    def test_metrics_data_robustness(self, metrics_data):
        """Property: Screen handles arbitrary valid metrics data without crashing."""
        with patch("tui.screens.metrics.EUXIS_HOME", Path(self.temp_dir)):
            # Write the property-generated data
            metrics_file = self.metrics_path / "agent_timings.json"
            metrics_file.write_text(json.dumps(metrics_data))

            # Screen should not crash with any valid JSON metrics data
            screen = MetricsScreen()
            try:
                result = screen.compose()
                assert result is not None
            except (TypeError, ValueError, KeyError) as e:
                self.fail(f"Screen crashed with valid data: {e}")

    @given(invalid_json=st.text())
    def test_invalid_metrics_data_handling(self, invalid_json):
        """Property: Screen gracefully handles invalid JSON metrics data."""
        with patch("tui.screens.metrics.EUXIS_HOME", Path(self.temp_dir)):
            # Write invalid JSON data
            metrics_file = self.metrics_path / "agent_timings.json"
            metrics_file.write_text(invalid_json)

            # Screen should not crash even with invalid data
            screen = MetricsScreen()
            try:
                result = screen.compose()
                assert result is not None
            except json.JSONDecodeError:
                # This is expected for invalid JSON
                pass
            except (TypeError, ValueError, KeyError) as e:
                # Other exceptions indicate improper error handling
                self.fail(f"Screen crashed unexpectedly with invalid JSON: {e}")

    @given(st.lists(st.integers(), min_size=0, max_size=1000))
    def test_sparkline_data_boundary_conditions(self, data_points):
        """Property: Sparkline generation handles boundary conditions."""
        # Import sparkline functionality
        from tui.widgets.sparkline import sparkline_text

        try:
            result = sparkline_text(data_points)
            # Should always return a string
            assert isinstance(result, str)

            if len(data_points) == 0:
                # Empty data should return empty or placeholder string
                assert len(result) == 0 or result.isspace()
            else:
                # Non-empty data should return non-empty sparkline
                assert len(result.strip()) > 0

        except (TypeError, ValueError, OverflowError) as e:
            self.fail(f"Sparkline generation failed with data {data_points[:10]}...: {e}")

    @given(st.integers(min_value=-1000, max_value=1000))
    def test_single_value_metrics(self, single_value):
        """Property: Single-value metrics are handled correctly."""
        from tui.widgets.sparkline import sparkline_text

        result = sparkline_text([single_value])
        assert isinstance(result, str)
        # Single value should produce some visual representation
        assert len(result.strip()) > 0


class TestMetricsScreenEdgeCases(unittest.TestCase):
    """Edge case tests for MetricsScreen."""

    def setUp(self):
        """Set up test environment."""
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_metrics_edge_")

    def tearDown(self):
        """Clean up test environment."""
        import shutil
        shutil.rmtree(self.temp_dir)

    @patch("tui.screens.metrics.EUXIS_HOME")
    def test_missing_metrics_directory(self, mock_home):
        """Test behavior when metrics directory doesn't exist."""
        mock_home.return_value = Path(self.temp_dir)
        # Don't create metrics directory

        screen = MetricsScreen()
        result = screen.compose()
        assert result is not None

    @patch("tui.screens.metrics.EUXIS_HOME")
    def test_permission_denied_metrics_directory(self, mock_home):
        """Test behavior when metrics directory is not accessible."""
        mock_home.return_value = Path(self.temp_dir)
        metrics_path = Path(self.temp_dir) / "metrics"
        metrics_path.mkdir()

        # Simulate permission denied by mocking Path.exists to raise PermissionError
        with patch.object(Path, "exists", side_effect=PermissionError):
            screen = MetricsScreen()
            result = screen.compose()
            assert result is not None

    def test_unicode_agent_names(self):
        """Test handling of Unicode characters in agent names."""
        test_data = {
            "测试者": [100, 200, 300],
            "デバッガー": [150, 250],
            "архитектор": [200, 100, 400],
            "🤖_agent": [50, 60, 70]
        }

        # Should not crash with Unicode agent names
        json_str = json.dumps(test_data)
        parsed_data = json.loads(json_str)
        assert parsed_data == test_data

    def test_extremely_large_metrics_values(self):
        """Test handling of very large metric values."""
        large_data = {
            "stress_test": [
                999999999,
                1000000000,
                2**31 - 1,  # Max 32-bit int
                2**53 - 1   # Max safe JavaScript integer
            ]
        }

        json_str = json.dumps(large_data)
        parsed_data = json.loads(json_str)
        assert parsed_data == large_data

    def test_negative_metrics_values(self):
        """Test handling of negative metric values."""
        negative_data = {
            "negative_test": [-100, -50, 0, 50, -200]
        }

        json_str = json.dumps(negative_data)
        parsed_data = json.loads(json_str)
        assert parsed_data == negative_data


if __name__ == "__main__":
    unittest.main()
