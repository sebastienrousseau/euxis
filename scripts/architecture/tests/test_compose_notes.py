# SPDX-License-Identifier: AGPL-3.0-or-later
"""Unit tests for scripts/release/compose_notes.py (no git or gh calls)."""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
_SPEC = importlib.util.spec_from_file_location(
    "compose_notes", REPO_ROOT / "scripts" / "release" / "compose_notes.py")
cn = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(cn)

SUM = "a" * 64 + "  euxis-v0.0.18-linux-amd64.tar.gz"
ENTRY = {"title": "fix: x", "author": "sebastienrousseau",
         "url": "https://github.com/sebastienrousseau/euxis/pull/1"}


class PreviousTag(unittest.TestCase):
    def test_skips_newer_and_non_release_tags(self) -> None:
        tags = ["v0.1.1", "v0.1.0", "v0.0.16", "v0.0.15", "v0.0.9", "nightly"]
        self.assertEqual(cn.previous_tag("v0.0.17", tags), "v0.0.16")
        self.assertEqual(cn.previous_tag("v0.0.16", tags), "v0.0.15")
        self.assertEqual(cn.previous_tag("v0.0.10", tags), "v0.0.9")

    def test_first_release_has_none(self) -> None:
        self.assertIsNone(cn.previous_tag("v0.0.1", ["v0.0.1", "v0.0.2"]))


class Pieces(unittest.TestCase):
    def test_plain_replaces_em_dashes(self) -> None:
        self.assertEqual(cn.plain("fix(ci): a — b"), "fix(ci): a - b")
        self.assertEqual(cn.plain("a—b"), "a - b")

    def test_highlights_keep_only_bullets(self) -> None:
        text = "# euxis 0.0.18: highlights\n\nIntro.\n\n* **A**: one.\n* **B**: two.\n"
        self.assertEqual(cn.highlight_lines(text), "* **A**: one.\n* **B**: two.")

    def test_highlights_without_bullets_fail(self) -> None:
        with self.assertRaises(cn.NotesError):
            cn.highlight_lines("# heading only\n")

    def test_sums_must_be_sha256_lines(self) -> None:
        self.assertEqual(cn.sum_lines(SUM + "\n\n"), SUM)
        for bad in ("", "abc  file", SUM + "\nnot a sum"):
            with self.assertRaises(cn.NotesError, msg=bad):
                cn.sum_lines(bad)


class Compose(unittest.TestCase):
    def test_sections_in_standard_order(self) -> None:
        notes = cn.compose("v0.0.18", "v0.0.17", "* **A**: one.", [ENTRY], [], SUM)
        order = ["## Highlights ⭐️", "## What's Changed", "## Checksums", "**Full Changelog**"]
        positions = [notes.index(h) for h in order]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn("## New Contributors", notes)
        self.assertIn("* fix: x by @sebastienrousseau in https://github.com/sebastienrousseau/euxis/pull/1",
                      notes)
        self.assertIn("SHA-256 of every asset attached to this release:\n\n```text\n" + SUM + "\n```", notes)
        self.assertIn("compare/v0.0.17...v0.0.18", notes)

    def test_new_contributors_section_when_present(self) -> None:
        notes = cn.compose("v0.0.18", "v0.0.17", "* **A**: one.", [ENTRY], [ENTRY], SUM)
        self.assertLess(notes.index("## New Contributors"), notes.index("## Checksums"))

    def test_first_release_links_commits(self) -> None:
        notes = cn.compose("v0.0.1", None, "* **A**: one.", [ENTRY], [], SUM)
        self.assertIn("**Full Changelog**: https://github.com/sebastienrousseau/euxis/commits/v0.0.1", notes)

    def test_em_dash_anywhere_fails(self) -> None:
        with self.assertRaises(cn.NotesError):
            cn.compose("v0.0.18", "v0.0.17", "* **A**: one — two.", [ENTRY], [], SUM)


if __name__ == "__main__":
    unittest.main()
