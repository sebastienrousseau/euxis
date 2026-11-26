# SPDX-License-Identifier: AGPL-3.0-or-later
"""Unit tests for scripts/release/preflight.py (no git or gh calls)."""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
_SPEC = importlib.util.spec_from_file_location(
    "release_preflight", REPO_ROOT / "scripts" / "release" / "preflight.py")
pf = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(pf)

SIGNED = "object abc\ntype commit\ntag v0.0.18\n\neuxis v0.0.18\n-----BEGIN SSH SIGNATURE-----\nx\n"
CMAKE = "cmake_minimum_required(VERSION 3.28)\nproject(euxis VERSION 0.0.18 LANGUAGES CXX)\n"
CHANGELOG = "# Changelog\n\n## [Unreleased]\n\n## [0.0.18] - 2026-11-26\n\n### Security\n"
HIGHLIGHTS = "# euxis 0.0.18: highlights\n\n* **A**: one.\n"


class TagObject(unittest.TestCase):
    def test_signed_annotated_with_project_message_passes(self) -> None:
        pf.check_tag_object("v0.0.18", "tag", SIGNED, "euxis v0.0.18")

    def test_rejects_lightweight_unsigned_and_bare_message(self) -> None:
        cases = [("commit", SIGNED, "euxis v0.0.18"),
                 ("tag", SIGNED.replace("-----BEGIN SSH SIGNATURE-----", ""), "euxis v0.0.18"),
                 ("tag", SIGNED, "v0.0.18")]
        for kind, raw, subject in cases:
            with self.assertRaises(pf.PreflightError, msg=(kind, subject)):
                pf.check_tag_object("v0.0.18", kind, raw, subject)

    def test_rejects_non_release_tag(self) -> None:
        for tag in ("0.0.18", "v0.0", "v0.0.18-rc1"):
            with self.assertRaises(pf.PreflightError, msg=tag):
                pf.version_of(tag)


class Files(unittest.TestCase):
    def test_current_carriers_pass(self) -> None:
        pf.check_files("v0.0.18", CMAKE, CHANGELOG, HIGHLIGHTS)

    def test_stale_cmake_version_fails(self) -> None:
        with self.assertRaises(pf.PreflightError):
            pf.check_files("v0.0.18", CMAKE.replace("0.0.18", "0.0.17"), CHANGELOG, HIGHLIGHTS)

    def test_changelog_needs_a_dated_section(self) -> None:
        for text in (CHANGELOG.replace(" - 2026-11-26", ""), "## [Unreleased]\n"):
            with self.assertRaises(pf.PreflightError, msg=text):
                pf.check_files("v0.0.18", CMAKE, text, HIGHLIGHTS)

    def test_highlights_need_bullets(self) -> None:
        with self.assertRaises(pf.PreflightError):
            pf.check_files("v0.0.18", CMAKE, CHANGELOG, "# heading only\n")

    def test_legacy_skips_changelog_and_highlights(self) -> None:
        pf.check_files("v0.0.18", CMAKE, None, None)


class Remote(unittest.TestCase):
    def test_zero_open_passes(self) -> None:
        pf.check_remote([], [])

    def test_any_open_alert_or_issue_fails(self) -> None:
        for alerts, issues in (([{}], []), ([], [{}])):
            with self.assertRaises(pf.PreflightError):
                pf.check_remote(alerts, issues)


if __name__ == "__main__":
    unittest.main()
