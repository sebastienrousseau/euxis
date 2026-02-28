# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

from pathlib import Path

import pytest

import check_tech_debt as debt


def _write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def test_check_debt_passes_within_limits(monkeypatch, tmp_path, capsys) -> None:
    repo_root = tmp_path / "repo"
    _write(repo_root / "pkg1" / "src" / "a.py", "print('ok')\n# pragma: no cover\n")
    _write(repo_root / "pkg2" / "src" / "b.sh", "#!/bin/sh\n# TODO: later\n")

    monkeypatch.setattr(debt, "search_dirs", ["pkg1", "pkg2"])
    monkeypatch.setattr(debt, "MAX_PRAGMAS", 2)
    monkeypatch.setattr(debt, "MAX_TODOS", 2)
    monkeypatch.setattr(debt, "__file__", str(repo_root / "euxis-scripts" / "check_tech_debt.py"))

    with pytest.raises(SystemExit) as exc:
        debt.check_debt()
    assert exc.value.code == 0
    out = capsys.readouterr().out
    assert "Current # pragma: no cover count: 1" in out
    assert "Current TODO/FIXME count: 1" in out


def test_check_debt_fails_when_limits_exceeded(monkeypatch, tmp_path, capsys) -> None:
    repo_root = tmp_path / "repo"
    _write(repo_root / "pkg" / "src" / "c.py", "# pragma: no cover\n# FIXME: x\n")

    monkeypatch.setattr(debt, "search_dirs", ["pkg"])
    monkeypatch.setattr(debt, "MAX_PRAGMAS", 0)
    monkeypatch.setattr(debt, "MAX_TODOS", 0)
    monkeypatch.setattr(debt, "__file__", str(repo_root / "euxis-scripts" / "check_tech_debt.py"))

    with pytest.raises(SystemExit) as exc:
        debt.check_debt()
    assert exc.value.code == 1
    out = capsys.readouterr().out
    assert "exceeds limit" in out


def test_check_debt_skips_tests_and_handles_read_errors(monkeypatch, tmp_path, capsys) -> None:
    repo_root = tmp_path / "repo"
    _write(repo_root / "pkg" / "src" / "tests" / "ignored.py", "# TODO ignored\n# pragma: no cover\n")
    _write(repo_root / "pkg" / "src" / "ok.py", "print('ok')\n")
    unreadable = repo_root / "pkg" / "src" / "bad.py"
    _write(unreadable, "print('x')\n")

    real_open = open

    def fake_open(path, *args, **kwargs):
        if str(path).endswith("bad.py"):
            raise OSError("denied")
        return real_open(path, *args, **kwargs)

    monkeypatch.setattr(debt, "search_dirs", ["pkg"])
    monkeypatch.setattr(debt, "MAX_PRAGMAS", 0)
    monkeypatch.setattr(debt, "MAX_TODOS", 0)
    monkeypatch.setattr(debt, "__file__", str(repo_root / "euxis-scripts" / "check_tech_debt.py"))
    monkeypatch.setattr("builtins.open", fake_open)

    with pytest.raises(SystemExit) as exc:
        debt.check_debt()
    assert exc.value.code == 0
    out = capsys.readouterr().out
    assert "count: 0" in out


def test_check_debt_flat_project_and_missing_dir_and_extension_filter(monkeypatch, tmp_path, capsys) -> None:
    repo_root = tmp_path / "repo"
    # Flat project (no src dir) should be scanned via fallback path.
    _write(repo_root / "flatpkg" / "main.py", "# pragma: no cover\n")
    # Non-Python/Shell file should be ignored by extension filter.
    _write(repo_root / "flatpkg" / "notes.txt", "TODO should be ignored\n")
    # Missing package should be skipped by continue branch.
    # Explicit read error to hit exception path.
    _write(repo_root / "flatpkg" / "broken.py", "print('x')\n")

    real_open = open

    def fake_open(path, *args, **kwargs):
        if str(path).endswith("broken.py"):
            raise OSError("denied")
        return real_open(path, *args, **kwargs)

    monkeypatch.setattr(debt, "search_dirs", ["flatpkg", "missingpkg"])
    monkeypatch.setattr(debt, "MAX_PRAGMAS", 1)
    monkeypatch.setattr(debt, "MAX_TODOS", 0)
    monkeypatch.setattr(debt, "__file__", str(repo_root / "euxis-scripts" / "check_tech_debt.py"))
    monkeypatch.setattr("builtins.open", fake_open)

    with pytest.raises(SystemExit) as exc:
        debt.check_debt()
    assert exc.value.code == 0
    out = capsys.readouterr().out
    assert "Current # pragma: no cover count: 1" in out
    assert "Current TODO/FIXME count: 0" in out
