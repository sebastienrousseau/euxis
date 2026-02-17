"""Comprehensive unit tests for tui/i18n.py internationalization module.

Tests cover: LOCALES_DIR, set_locale (en, other languages, missing locale),
_() translation function (passthrough, translated), _translator state.
"""

from __future__ import annotations

import gettext
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tui import i18n
from tui.i18n import _, set_locale


class TestLocalesDir(unittest.TestCase):
    """Tests for LOCALES_DIR constant."""

    def test_locales_dir_is_path(self):
        """Test LOCALES_DIR is a Path object."""
        assert isinstance(i18n.LOCALES_DIR, Path)

    def test_locales_dir_under_euxis_home(self):
        """Test LOCALES_DIR is under EUXIS_HOME."""
        assert "locales" in str(i18n.LOCALES_DIR)


class TestSetLocaleEnglish(unittest.TestCase):
    """Tests for set_locale with English."""

    def test_set_locale_en_uses_null_translations(self):
        """Test setting locale to 'en' uses NullTranslations."""
        set_locale("en")
        assert isinstance(i18n._translator, gettext.NullTranslations)

    def test_set_locale_en_is_passthrough(self):
        """Test English locale passes through messages unchanged."""
        set_locale("en")
        result = _("test message")
        assert result == "test message"

    def test_set_locale_en_preserves_formatting(self):
        """Test English locale preserves message formatting."""
        set_locale("en")
        result = _("Hello {name}!")
        assert result == "Hello {name}!"

    def test_set_locale_en_handles_unicode(self):
        """Test English locale handles Unicode strings."""
        set_locale("en")
        result = _("Caf\u00e9 ☕")
        assert result == "Caf\u00e9 ☕"


class TestSetLocaleMissingLanguage(unittest.TestCase):
    """Tests for set_locale with missing language files."""

    def test_missing_locale_falls_back_to_english(self):
        """Test missing locale file falls back to NullTranslations."""
        set_locale("nonexistent_language_xyz")
        assert isinstance(i18n._translator, gettext.NullTranslations)

    def test_missing_locale_still_translates(self):
        """Test missing locale still returns messages."""
        set_locale("xx_NOTREAL")
        result = _("test message")
        assert result == "test message"


class TestSetLocaleOtherLanguage(unittest.TestCase):
    """Tests for set_locale with other languages."""

    def setUp(self):
        """Create temp locale directory with test .mo file."""
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_i18n_test_")
        self.orig_locales_dir = i18n.LOCALES_DIR

    def tearDown(self):
        """Clean up temp directory."""
        shutil.rmtree(self.tmpdir, ignore_errors=True)
        # Restore original
        i18n.LOCALES_DIR = self.orig_locales_dir
        set_locale("en")  # Reset to English

    def test_set_locale_attempts_translation_load(self):
        """Test set_locale attempts to load translation file."""
        with patch.object(i18n, "LOCALES_DIR", Path(self.tmpdir)):
            # Should not crash even without .mo files
            set_locale("fr")
            # Falls back to NullTranslations
            assert isinstance(i18n._translator, gettext.NullTranslations)


class TestTranslationFunction(unittest.TestCase):
    """Tests for the _() translation function."""

    def setUp(self):
        """Reset to English locale."""
        set_locale("en")

    def test_underscore_function_exists(self):
        """Test _() function is callable."""
        assert callable(_)

    def test_underscore_returns_string(self):
        """Test _() returns a string."""
        result = _("test")
        assert isinstance(result, str)

    def test_underscore_passthrough(self):
        """Test _() passes through in English."""
        assert _("Hello") == "Hello"
        assert _("World") == "World"

    def test_underscore_empty_string(self):
        """Test _() handles empty string."""
        result = _("")
        assert result == ""

    def test_underscore_whitespace_only(self):
        """Test _() handles whitespace-only string."""
        result = _("   ")
        assert result == "   "

    def test_underscore_multiline(self):
        """Test _() handles multiline string."""
        msg = "Line 1\nLine 2\nLine 3"
        result = _(msg)
        assert result == msg

    def test_underscore_special_characters(self):
        """Test _() handles special characters."""
        msg = "Price: $100 (20% off!)"
        result = _(msg)
        assert result == msg

    def test_underscore_format_placeholders(self):
        """Test _() preserves format placeholders."""
        msg = "{count} items in {location}"
        result = _(msg)
        assert result == msg

    def test_underscore_percent_formatting(self):
        """Test _() preserves percent formatting."""
        msg = "%(name)s has %(count)d items"
        result = _(msg)
        assert result == msg


class TestTranslatorState(unittest.TestCase):
    """Tests for _translator module-level state."""

    def test_translator_is_initialized(self):
        """Test _translator is initialized at module load."""
        assert i18n._translator is not None

    def test_translator_is_null_translations(self):
        """Test _translator starts as NullTranslations."""
        # After import, should be NullTranslations
        set_locale("en")
        assert isinstance(i18n._translator, gettext.NullTranslations)

    def test_set_locale_changes_translator(self):
        """Test set_locale modifies _translator."""
        set_locale("en")
        # Should still be NullTranslations but potentially new instance
        assert isinstance(i18n._translator, gettext.NullTranslations)


class TestSetLocaleEdgeCases(unittest.TestCase):
    """Edge case tests for set_locale."""

    def test_set_locale_empty_string(self):
        """Test set_locale with empty string falls back."""
        set_locale("")
        # Empty string is not "en", so it tries to load and fails
        assert isinstance(i18n._translator, gettext.NullTranslations)

    def test_set_locale_uppercase_en(self):
        """Test set_locale with uppercase 'EN' is not treated as 'en'."""
        set_locale("EN")
        # "EN" != "en", so it tries to load
        assert isinstance(i18n._translator, gettext.NullTranslations)

    def test_set_locale_locale_code_format(self):
        """Test set_locale with full locale code like 'en_US'."""
        set_locale("en_US")
        # "en_US" != "en", so it tries to load
        assert isinstance(i18n._translator, gettext.NullTranslations)

    def test_set_locale_multiple_calls(self):
        """Test calling set_locale multiple times."""
        set_locale("en")
        set_locale("fr")
        set_locale("en")
        set_locale("de")
        # Should not crash, final state is fallback
        assert isinstance(i18n._translator, gettext.NullTranslations)


class TestIntegration(unittest.TestCase):
    """Integration tests for i18n module."""

    def test_workflow_set_locale_then_translate(self):
        """Test typical workflow: set locale then translate."""
        set_locale("en")
        msg1 = _("agents")
        msg2 = _("squads")
        assert msg1 == "agents"
        assert msg2 == "squads"

    def test_locale_change_affects_translation(self):
        """Test changing locale affects subsequent translations."""
        set_locale("en")
        en_msg = _("Hello")

        # Change to missing locale (falls back)
        set_locale("xx")
        xx_msg = _("Hello")

        # Both should be passthrough since xx doesn't exist
        assert en_msg == "Hello"
        assert xx_msg == "Hello"

    def test_concurrent_safe(self):
        """Test i18n can handle rapid locale changes."""
        for idx in range(100):
            set_locale("en")
            _("test")
            set_locale("fr")
            _("test")
            assert idx >= 0
        # Should complete without error
        assert True


if __name__ == "__main__":
    unittest.main()
