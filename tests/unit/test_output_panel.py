"""Comprehensive unit tests for OutputPanel.

Tests cover: write_line all 10 format branches ([euxis] prefix + spinner strip,
ERROR/WARNING/PASS/FAIL, heading #, code fence, list item, plain),
write_status, write_separator, hypothesis robustness.
"""

from __future__ import annotations

import unittest
from unittest.mock import Mock, patch

from hypothesis import given, settings
from hypothesis import strategies as st

from tui.widgets.output_panel import OutputPanel


class TestOutputPanelInit(unittest.TestCase):
    @patch("tui.widgets.output_panel.RichLog.__init__", return_value=None)
    def test_init_passes_kwargs(self, mock_init):
        panel = OutputPanel.__new__(OutputPanel)
        OutputPanel.__init__(panel, id="test")
        mock_init.assert_called_once()
        kwargs = mock_init.call_args[1]
        assert kwargs["highlight"] is True
        assert kwargs["markup"] is True
        assert kwargs["wrap"] is True


class TestOutputPanelWriteLine(unittest.TestCase):
    """Test all format branches in write_line."""

    def setUp(self):
        self.panel = OutputPanel.__new__(OutputPanel)
        self.panel.write = Mock()

    def test_euxis_prefix_styled(self):
        self.panel.write_line("[euxis] Loading agents...")
        call_arg = self.panel.write.call_args[0][0]
        assert "bold cyan" in str(call_arg._spans) or "cyan" in str(call_arg)

    def test_euxis_prefix_strips_spinner(self):
        self.panel.write_line("[euxis] ⠋ Loading...")
        call_arg = self.panel.write.call_args[0][0]
        # Spinner character should be stripped
        assert "⠋" not in call_arg.plain

    def test_error_prefix(self):
        self.panel.write_line("ERROR: something failed")
        call_arg = self.panel.write.call_args[0][0]
        assert "red" in str(call_arg._spans) or "red" in str(call_arg)

    def test_error_bracket_prefix(self):
        self.panel.write_line("[ERROR] something failed")
        call_arg = self.panel.write.call_args[0][0]
        assert "red" in str(call_arg._spans) or "red" in str(call_arg)

    def test_warning_prefix(self):
        self.panel.write_line("WARNING: check this")
        call_arg = self.panel.write.call_args[0][0]
        assert "yellow" in str(call_arg._spans) or "yellow" in str(call_arg)

    def test_warning_bracket_prefix(self):
        self.panel.write_line("[WARNING] check this")
        call_arg = self.panel.write.call_args[0][0]
        assert "yellow" in str(call_arg._spans) or "yellow" in str(call_arg)

    def test_pass_prefix(self):
        self.panel.write_line("PASS: all tests passed")
        call_arg = self.panel.write.call_args[0][0]
        assert "green" in str(call_arg._spans) or "green" in str(call_arg)

    def test_pass_checkmark(self):
        self.panel.write_line("✅ all good")
        call_arg = self.panel.write.call_args[0][0]
        assert "green" in str(call_arg._spans) or "green" in str(call_arg)

    def test_fail_prefix(self):
        self.panel.write_line("FAIL: test broke")
        call_arg = self.panel.write.call_args[0][0]
        assert "red" in str(call_arg._spans) or "red" in str(call_arg)

    def test_fail_x_mark(self):
        self.panel.write_line("✗ failed check")
        call_arg = self.panel.write.call_args[0][0]
        assert "red" in str(call_arg._spans) or "red" in str(call_arg)

    def test_heading(self):
        self.panel.write_line("# My Heading")
        call_arg = self.panel.write.call_args[0][0]
        assert "bold" in str(call_arg._spans) or "bold" in str(call_arg)

    def test_code_fence(self):
        self.panel.write_line("```python")
        call_arg = self.panel.write.call_args[0][0]
        assert "dim" in str(call_arg._spans) or "dim" in str(call_arg)

    def test_list_item_dash(self):
        self.panel.write_line("- item one")
        call_arg = self.panel.write.call_args[0][0]
        assert "- item one" in call_arg.plain

    def test_list_item_asterisk(self):
        self.panel.write_line("* item two")
        call_arg = self.panel.write.call_args[0][0]
        assert "* item two" in call_arg.plain

    def test_plain_text(self):
        self.panel.write_line("just regular text")
        self.panel.write.assert_called_once_with("just regular text")

    def test_empty_line(self):
        self.panel.write_line("")
        self.panel.write.assert_called_once_with("")


class TestOutputPanelWriteStatus(unittest.TestCase):
    def setUp(self):
        self.panel = OutputPanel.__new__(OutputPanel)
        self.panel.write = Mock()

    def test_default_style(self):
        self.panel.write_status("Loading...")
        call_arg = self.panel.write.call_args[0][0]
        assert "Loading..." in call_arg.plain

    def test_custom_style(self):
        self.panel.write_status("Error occurred", "red")
        call_arg = self.panel.write.call_args[0][0]
        assert "Error occurred" in call_arg.plain


class TestOutputPanelWriteSeparator(unittest.TestCase):
    def setUp(self):
        self.panel = OutputPanel.__new__(OutputPanel)
        self.panel.write = Mock()

    def test_separator(self):
        self.panel.write_separator()
        call_arg = self.panel.write.call_args[0][0]
        assert "─" in call_arg.plain
        assert len(call_arg.plain) == 60


class TestOutputPanelHypothesis(unittest.TestCase):
    """Property-based tests for write_line robustness."""

    def setUp(self):
        self.panel = OutputPanel.__new__(OutputPanel)
        self.panel.write = Mock()

    @settings(max_examples=100)
    @given(line=st.text(max_size=500))
    def test_arbitrary_text_never_crashes(self, line):
        """write_line should never raise for any input string."""
        self.panel.write_line(line)

    @settings(max_examples=50)
    @given(
        message=st.text(min_size=1, max_size=100),
        style=st.sampled_from(["cyan", "red", "green", "yellow", "dim", "bold"]),
    )
    def test_write_status_never_crashes(self, message, style):
        self.panel.write_status(message, style)


if __name__ == "__main__":
    unittest.main()
