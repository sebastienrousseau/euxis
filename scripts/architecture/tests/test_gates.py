# SPDX-License-Identifier: AGPL-3.0-or-later
"""Unit tests for the architecture-quality gate scripts.

These tests run via plain `python3 -m unittest` so they need no extra
dependencies and can be invoked from the same workflow that runs the
gates themselves.
"""
from __future__ import annotations

import contextlib
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
SCRIPTS_DIR = REPO_ROOT / "scripts" / "architecture"


def run_script(name: str, *args: str, cwd: Path | None = None
               ) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPTS_DIR / name), *args],
        cwd=cwd or REPO_ROOT,
        capture_output=True,
        text=True,
    )


@contextlib.contextmanager
def temp_git_repo():
    """Yield a clean, isolated git repository."""
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        subprocess.run(["git", "init", "-q"], cwd=root, check=True)
        subprocess.run(["git", "config", "user.email", "test@example.com"],
                       cwd=root, check=True)
        subprocess.run(["git", "config", "user.name", "Test"],
                       cwd=root, check=True)
        subprocess.run(["git", "config", "commit.gpgsign", "false"],
                       cwd=root, check=True)
        yield root


class NoStaleDirsTest(unittest.TestCase):
    def test_clean_repo_passes(self):
        with temp_git_repo() as root:
            (root / "src.cpp").write_text("// hi")
            subprocess.run(["git", "add", "."], cwd=root, check=True)
            subprocess.run(["git", "commit", "-m", "init", "-q"],
                           cwd=root, check=True)
            result = run_script("check_no_stale_dirs.py",
                                "--repo-root", str(root))
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_tracked_tui_dir_fails(self):
        with temp_git_repo() as root:
            (root / "tui").mkdir()
            (root / "tui" / "app.py").write_text("# stale")
            subprocess.run(["git", "add", "tui/app.py"], cwd=root, check=True)
            subprocess.run(["git", "commit", "-m", "x", "-q"],
                           cwd=root, check=True)
            result = run_script("check_no_stale_dirs.py",
                                "--repo-root", str(root))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("tui/app.py", result.stderr)

    def test_pyc_file_fails(self):
        with temp_git_repo() as root:
            (root / "x.pyc").write_text("bytes")
            # Simulate the scenario the gate exists to catch: a
            # contributor used `git add -f` to bypass .gitignore.
            subprocess.run(["git", "add", "-f", "x.pyc"],
                           cwd=root, check=True)
            subprocess.run(["git", "commit", "-m", "x", "-q"],
                           cwd=root, check=True)
            result = run_script("check_no_stale_dirs.py",
                                "--repo-root", str(root))
            self.assertNotEqual(result.returncode, 0)


class LibLayoutTest(unittest.TestCase):
    def _make_lib(self, root: Path, name: str, *,
                  cmake: bool = True,
                  header: bool = True,
                  src: bool = True,
                  tests: bool = True) -> None:
        lib = root / "libs" / name
        lib.mkdir(parents=True, exist_ok=True)
        if cmake:
            (lib / "CMakeLists.txt").write_text("# stub")
        if header:
            header_dir = lib / "include" / "euxis" / name
            header_dir.mkdir(parents=True)
            (header_dir / "x.hpp").write_text("#pragma once")
        if src:
            src_dir = lib / "src"
            src_dir.mkdir(parents=True)
            (src_dir / "x.cpp").write_text("// x")
        if tests:
            test_dir = lib / "tests"
            test_dir.mkdir(parents=True)
            (test_dir / "test_x.cpp").write_text("// t")

    def test_canonical_layout_passes(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            self._make_lib(root, "widget")
            result = run_script("check_lib_layout.py",
                                "--repo-root", str(root))
            self.assertEqual(result.returncode, 0,
                             result.stderr + result.stdout)

    def test_missing_cmake_fails(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            self._make_lib(root, "widget", cmake=False)
            result = run_script("check_lib_layout.py",
                                "--repo-root", str(root))
            self.assertNotEqual(result.returncode, 0)

    def test_missing_header_fails(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            self._make_lib(root, "widget", header=False)
            result = run_script("check_lib_layout.py",
                                "--repo-root", str(root))
            self.assertNotEqual(result.returncode, 0)

    def test_missing_tests_warns_but_passes(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            self._make_lib(root, "widget", tests=False)
            result = run_script("check_lib_layout.py",
                                "--repo-root", str(root))
            self.assertEqual(result.returncode, 0,
                             result.stderr + result.stdout)

    def test_missing_tests_strict_fails(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            self._make_lib(root, "widget", tests=False)
            result = run_script("check_lib_layout.py",
                                "--repo-root", str(root), "--strict")
            self.assertNotEqual(result.returncode, 0)


class WorkflowIntegrityTest(unittest.TestCase):
    def test_clean_workflow_passes(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            wf = root / ".github" / "workflows"
            wf.mkdir(parents=True)
            (wf / "ok.yml").write_text(
                "jobs:\n"
                "  a:\n"
                "    steps:\n"
                "      - run: python3 scripts/x.py\n"
            )
            (root / "scripts").mkdir()
            (root / "scripts" / "x.py").write_text("# stub")
            result = run_script("check_workflow_integrity.py",
                                "--repo-root", str(root))
            self.assertEqual(result.returncode, 0,
                             result.stderr + result.stdout)

    def test_missing_script_fails(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            wf = root / ".github" / "workflows"
            wf.mkdir(parents=True)
            (wf / "bad.yml").write_text(
                "jobs:\n"
                "  a:\n"
                "    steps:\n"
                "      - run: python3 scripts/missing.py\n"
            )
            result = run_script("check_workflow_integrity.py",
                                "--repo-root", str(root))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("scripts/missing.py", result.stderr)


class BrandingCheckTest(unittest.TestCase):
    def _make_repo_with_signature(self) -> Path:
        d = tempfile.TemporaryDirectory()
        root = Path(d.name)
        # Keep d alive
        self.addCleanup(d.cleanup)
        (root / "data" / "config" / "branding").mkdir(parents=True)
        (root / "data" / "config" / "branding" / "signature.txt").write_text(
            "---\n"
            "THE TEST FOOTER LINE\n"
        )
        subprocess.run(["git", "init", "-q"], cwd=root, check=True)
        subprocess.run(["git", "config", "user.email", "t@e"],
                       cwd=root, check=True)
        subprocess.run(["git", "config", "user.name", "T"],
                       cwd=root, check=True)
        subprocess.run(["git", "config", "commit.gpgsign", "false"],
                       cwd=root, check=True)
        return root

    def test_branding_present_passes(self):
        root = self._make_repo_with_signature()
        (root / "f").write_text("x")
        subprocess.run(["git", "add", "."], cwd=root, check=True)
        subprocess.run(
            ["git", "commit", "-q", "-m",
             "feat: thing\n\nbody\n\nTHE TEST FOOTER LINE\n"],
            cwd=root, check=True)
        result = run_script("check_commit_branding.py",
                            "--repo-root", str(root))
        self.assertEqual(result.returncode, 0,
                         result.stderr + result.stdout)

    def test_branding_missing_fails(self):
        root = self._make_repo_with_signature()
        (root / "f").write_text("x")
        subprocess.run(["git", "add", "."], cwd=root, check=True)
        subprocess.run(
            ["git", "commit", "-q", "-m", "feat: thing\n\nno footer"],
            cwd=root, check=True)
        result = run_script("check_commit_branding.py",
                            "--repo-root", str(root))
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
