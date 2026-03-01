"""Tests for static analysis pattern detection."""

from __future__ import annotations

from pathlib import Path

import pytest

from euxis_bridge.core.static_analysis import (
    AnalysisFinding,
    AnalysisReport,
    analyze_file,
    analyze_skill_directory,
)


class TestAnalyzeFileJS:
    """Test JavaScript/TypeScript pattern detection."""

    def test_detects_eval(self, tmp_path: Path) -> None:
        js = tmp_path / "skill.js"
        js.write_text("const x = eval('1+1');\n")
        findings = analyze_file(js)
        assert len(findings) >= 1
        assert any(f.severity == "critical" and "eval" in f.description for f in findings)

    def test_detects_child_process(self, tmp_path: Path) -> None:
        js = tmp_path / "skill.js"
        js.write_text("const cp = require('child_process');\n")
        findings = analyze_file(js)
        assert any(f.severity == "critical" and "child_process" in f.description for f in findings)

    def test_detects_process_env(self, tmp_path: Path) -> None:
        js = tmp_path / "skill.js"
        js.write_text("const key = process.env.SECRET;\n")
        findings = analyze_file(js)
        assert any(f.severity == "warning" and "process.env" in f.description for f in findings)

    def test_detects_proto_pollution(self, tmp_path: Path) -> None:
        js = tmp_path / "skill.js"
        js.write_text("obj.__proto__.isAdmin = true;\n")
        findings = analyze_file(js)
        assert any(f.severity == "warning" and "prototype" in f.description for f in findings)

    def test_detects_function_constructor(self, tmp_path: Path) -> None:
        ts = tmp_path / "skill.ts"
        ts.write_text("const fn = new Function('return 1');\n")
        findings = analyze_file(ts)
        assert any(f.severity == "critical" and "Function()" in f.description for f in findings)

    def test_detects_require_fs(self, tmp_path: Path) -> None:
        js = tmp_path / "skill.mjs"
        js.write_text("const fs = require('fs')\n")
        findings = analyze_file(js)
        assert any(f.severity == "warning" and "filesystem" in f.description.lower() for f in findings)

    def test_clean_js_passes(self, tmp_path: Path) -> None:
        js = tmp_path / "clean.js"
        js.write_text("function add(a, b) { return a + b; }\n")
        findings = analyze_file(js)
        assert len(findings) == 0


class TestAnalyzeFilePython:
    """Test Python pattern detection."""

    def test_detects_exec(self, tmp_path: Path) -> None:
        py = tmp_path / "skill.py"
        py.write_text("exec('print(1)')\n")
        findings = analyze_file(py)
        assert any(f.severity == "critical" and "exec()" in f.description for f in findings)

    def test_detects_dunder_import(self, tmp_path: Path) -> None:
        py = tmp_path / "skill.py"
        py.write_text("mod = __import__('os')\n")
        findings = analyze_file(py)
        assert any(f.severity == "critical" and "__import__" in f.description for f in findings)

    def test_detects_subprocess(self, tmp_path: Path) -> None:
        py = tmp_path / "skill.py"
        py.write_text("import subprocess\n")
        findings = analyze_file(py)
        assert any(f.severity == "critical" and "subprocess" in f.description for f in findings)

    def test_detects_os_system(self, tmp_path: Path) -> None:
        py = tmp_path / "skill.py"
        py.write_text("os.system('rm -rf /')\n")
        findings = analyze_file(py)
        assert any(f.severity == "critical" and "os.system" in f.description for f in findings)

    def test_detects_os_popen(self, tmp_path: Path) -> None:
        py = tmp_path / "skill.py"
        py.write_text("os.popen('ls')\n")
        findings = analyze_file(py)
        assert any(f.severity == "critical" and "os.popen" in f.description for f in findings)

    def test_detects_ctypes(self, tmp_path: Path) -> None:
        py = tmp_path / "skill.py"
        py.write_text("import ctypes\n")
        findings = analyze_file(py)
        assert any(f.severity == "warning" and "ctypes" in f.description for f in findings)

    def test_clean_python_passes(self, tmp_path: Path) -> None:
        py = tmp_path / "clean.py"
        py.write_text("def add(a, b):\n    return a + b\n")
        findings = analyze_file(py)
        assert len(findings) == 0


class TestAnalyzeFileEdgeCases:
    """Test edge cases and unsupported files."""

    def test_unsupported_extension_returns_empty(self, tmp_path: Path) -> None:
        txt = tmp_path / "readme.txt"
        txt.write_text("eval('bad')\n")
        findings = analyze_file(txt)
        assert len(findings) == 0

    def test_nonexistent_file_returns_empty(self, tmp_path: Path) -> None:
        missing = tmp_path / "missing.py"
        findings = analyze_file(missing)
        assert len(findings) == 0

    def test_finding_includes_location(self, tmp_path: Path) -> None:
        py = tmp_path / "skill.py"
        py.write_text("x = 1\nexec('2')\n")
        findings = analyze_file(py)
        assert any("skill.py:2" in f.location for f in findings)


class TestAnalyzeSkillDirectory:
    """Test directory-level scanning."""

    def test_scans_mixed_files(self, tmp_path: Path) -> None:
        (tmp_path / "main.js").write_text("eval('x')\n")
        (tmp_path / "util.py").write_text("def safe(): pass\n")
        report = analyze_skill_directory(tmp_path)
        assert report.scanned_files == 2
        assert not report.passed  # eval is critical

    def test_clean_directory_passes(self, tmp_path: Path) -> None:
        (tmp_path / "main.js").write_text("console.log('ok');\n")
        (tmp_path / "util.py").write_text("print('ok')\n")
        report = analyze_skill_directory(tmp_path)
        assert report.passed is True
        assert report.scanned_files == 2
        assert len(report.findings) == 0

    def test_warning_only_still_passes(self, tmp_path: Path) -> None:
        (tmp_path / "main.js").write_text("const x = process.env.KEY;\n")
        report = analyze_skill_directory(tmp_path)
        assert report.passed is True
        assert len(report.findings) >= 1

    def test_nonexistent_directory_passes(self, tmp_path: Path) -> None:
        report = analyze_skill_directory(tmp_path / "nope")
        assert report.passed is True
        assert report.scanned_files == 0

    def test_nested_files_scanned(self, tmp_path: Path) -> None:
        sub = tmp_path / "sub" / "deep"
        sub.mkdir(parents=True)
        (sub / "evil.py").write_text("exec('boom')\n")
        report = analyze_skill_directory(tmp_path)
        assert report.scanned_files == 1
        assert not report.passed
