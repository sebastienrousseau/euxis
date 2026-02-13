# (c) 2026 Euxis Fleet. All rights reserved.
"""ETX localisation infrastructure.

Provides gettext-based i18n with _() translation function.
Falls back to English when no translation is available.
"""

from __future__ import annotations

import gettext
from pathlib import Path

EUXIS_HOME = Path.home() / ".euxis"
LOCALES_DIR = EUXIS_HOME / "locales"

# Ensure locales directory exists
LOCALES_DIR.mkdir(parents=True, exist_ok=True)

# Module-level translator — starts with NullTranslations (English passthrough)
_translator: gettext.NullTranslations = gettext.NullTranslations()


def set_locale(lang_code: str) -> None:
    """Switch the active locale.

    Args:
        lang_code: ISO 639-1 language code (e.g. "en", "fr", "de").
    """
    global _translator  # noqa: PLW0603
    if lang_code == "en":
        _translator = gettext.NullTranslations()
        return
    try:
        _translator = gettext.translation(
            "euxis",
            localedir=str(LOCALES_DIR),
            languages=[lang_code],
        )
    except FileNotFoundError:
        # No .mo file for this language — fall back to English
        _translator = gettext.NullTranslations()


def _(message: str) -> str:
    """Translate a UI string, falling back to English."""
    return _translator.gettext(message)
