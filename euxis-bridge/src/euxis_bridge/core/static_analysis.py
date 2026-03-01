"""Static analysis for detecting malicious patterns in skill code."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True, slots=True)
class AnalysisFinding:
    severity: str  # "critical", "warning", "info"
    pattern: str
    location: str
    description: str


@dataclass(frozen=True, slots=True)
class AnalysisReport:
    findings: tuple[AnalysisFinding, ...]
    passed: bool
    scanned_files: int


_JS_PATTERNS: list[tuple[str, str, str]] = [
    (r"\beval\s*\(", "critical", "eval() can execute arbitrary code"),
    (r"\bchild_process\b", "critical", "child_process enables command execution"),
    (r"\bprocess\.env\b", "warning", "process.env accesses environment variables"),
    (r"__proto__", "warning", "prototype pollution risk"),
    (r"\bFunction\s*\(", "critical", "Function() constructor can execute arbitrary code"),
    (r"\brequire\s*\(\s*['\"]fs['\"]", "warning", "filesystem access via require('fs')"),
]

_PY_PATTERNS: list[tuple[str, str, str]] = [
    (r"\bexec\s*\(", "critical", "exec() can execute arbitrary code"),
    (r"\b__import__\s*\(", "critical", "__import__() enables dynamic imports"),
    (r"\bsubprocess\b", "critical", "subprocess enables command execution"),
    (r"\bos\.system\s*\(", "critical", "os.system() enables command execution"),
    (r"\bos\.popen\s*\(", "critical", "os.popen() enables command execution"),
    (r"\bctypes\b", "warning", "ctypes can access native memory"),
]

def _patterns_for_ext(ext: str) -> list[tuple[str, str, str]]:
    if ext in {".js", ".mjs", ".cjs", ".ts"}:
        return _JS_PATTERNS
    if ext in {".py", ".pyw"}:
        return _PY_PATTERNS
    return []


def analyze_file(path: Path) -> list[AnalysisFinding]:
    findings: list[AnalysisFinding] = []
    ext = path.suffix.lower()
    patterns = _patterns_for_ext(ext)
    if not patterns:
        return findings

    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return findings

    for line_num, line in enumerate(content.splitlines(), start=1):
        for regex, severity, desc in patterns:
            if re.search(regex, line):
                findings.append(AnalysisFinding(
                    severity=severity,
                    pattern=regex,
                    location=f"{path.name}:{line_num}",
                    description=desc,
                ))
    return findings


def analyze_skill_directory(skill_dir: Path) -> AnalysisReport:
    all_findings: list[AnalysisFinding] = []
    scanned = 0
    if not skill_dir.exists():
        return AnalysisReport(findings=(), passed=True, scanned_files=0)

    for path in sorted(skill_dir.rglob("*")):
        if path.is_file() and _patterns_for_ext(path.suffix.lower()):
            scanned += 1
            all_findings.extend(analyze_file(path))

    has_critical = any(f.severity == "critical" for f in all_findings)
    return AnalysisReport(
        findings=tuple(all_findings),
        passed=not has_critical,
        scanned_files=scanned,
    )
