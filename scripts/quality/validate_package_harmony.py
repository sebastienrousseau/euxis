#!/usr/bin/env python3
"""
Repository-wide package harmony validation.

Enforces:
- Structural consistency using package standards.
- Duplicate code blocks across package source trees.
- Python cyclomatic-complexity regression limits.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


SKIP_PARTS = {
    ".git",
    ".venv",
    ".pytest_cache",
    "__pycache__",
    "node_modules",
    "dist",
    "build",
    "target",
    "coverage",
    "tests",
}

CODE_EXTENSIONS = {".py", ".rs", ".ts", ".js"}
PYTHON_EXTENSIONS = {".py"}
MIN_DUPLICATE_BLOCK_LINES = 30


@dataclass(frozen=True)
class Package:
    name: str
    path: Path
    required_files: Tuple[str, ...]


def _load_packages(standards_path: Path, repo_root: Path) -> List[Package]:
    payload = json.loads(standards_path.read_text(encoding="utf-8"))
    packages: List[Package] = []
    for item in payload.get("packages", []):
        path = repo_root / item["path"]
        required = tuple(item.get("required_files", []))
        packages.append(Package(name=item["name"], path=path, required_files=required))
    return packages


def _iter_source_files(package_path: Path) -> Iterable[Path]:
    source_root = package_path / "src"
    roots = [source_root] if source_root.exists() else [package_path]
    for root in roots:
        if not root.exists():
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d not in SKIP_PARTS]
            current = Path(dirpath)
            for filename in filenames:
                path = current / filename
                if path.suffix in CODE_EXTENSIONS:
                    yield path


def _normalize_lines(path: Path) -> List[str]:
    text = path.read_text(encoding="utf-8", errors="ignore")
    lines: List[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#") or stripped.startswith("//"):
            continue
        lines.append(stripped)
    return lines


def _duplicate_blocks(files: Iterable[Path]) -> Tuple[int, List[Dict[str, object]]]:
    seen: Dict[str, Tuple[str, int]] = {}
    findings: List[Dict[str, object]] = []
    for path in files:
        lines = _normalize_lines(path)
        if len(lines) < MIN_DUPLICATE_BLOCK_LINES:
            continue
        for index in range(len(lines) - MIN_DUPLICATE_BLOCK_LINES + 1):
            block = "\n".join(lines[index:index + MIN_DUPLICATE_BLOCK_LINES])
            digest = hashlib.sha1(block.encode("utf-8")).hexdigest()
            prior = seen.get(digest)
            if prior is None:
                seen[digest] = (str(path), index + 1)
                continue
            if prior[0] == str(path):
                continue
            findings.append(
                {
                    "hash": digest,
                    "first_file": prior[0],
                    "first_line": prior[1],
                    "second_file": str(path),
                    "second_line": index + 1,
                }
            )
    return len(findings), findings[:10]


def _function_complexity(node: ast.AST) -> int:
    complexity = 1
    for item in ast.walk(node):
        if isinstance(
            item,
            (
                ast.If,
                ast.For,
                ast.AsyncFor,
                ast.While,
                ast.Try,
                ast.ExceptHandler,
                ast.With,
                ast.AsyncWith,
                ast.BoolOp,
                ast.IfExp,
                ast.Match,
                ast.comprehension,
            ),
        ):
            complexity += 1
    return complexity


def _python_complexity(files: Iterable[Path]) -> Dict[str, object]:
    max_complexity = 0
    over_30 = 0
    over_40 = 0
    top_functions: List[Dict[str, object]] = []
    for path in files:
        if path.suffix not in PYTHON_EXTENSIONS:
            continue
        try:
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
        except SyntaxError:
            continue
        for node in ast.walk(tree):
            if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            score = _function_complexity(node)
            max_complexity = max(max_complexity, score)
            if score > 30:
                over_30 += 1
            if score > 40:
                over_40 += 1
            top_functions.append(
                {
                    "file": str(path),
                    "function": node.name,
                    "complexity": score,
                }
            )
    top_functions.sort(key=lambda item: int(item["complexity"]), reverse=True)
    return {
        "max_function_complexity": max_complexity,
        "functions_over_30": over_30,
        "functions_over_40": over_40,
        "top_functions": top_functions[:10],
    }


def _validate_structure(packages: Iterable[Package]) -> List[str]:
    issues: List[str] = []
    for package in packages:
        if not package.path.exists():
            issues.append(f"{package.name}: missing package path {package.path}")
            continue
        for required_file in package.required_files:
            required_path = package.path / required_file
            if not required_path.exists():
                issues.append(
                    f"{package.name}: missing required file {required_path}"
                )
    return issues


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument(
        "--standards",
        default="scripts/quality/package_standards.json",
    )
    parser.add_argument(
        "--baseline",
        default="scripts/quality/package_harmony_baseline.json",
    )
    parser.add_argument(
        "--json-output",
        default="data/release/package-harmony.json",
    )
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    standards_path = repo_root / args.standards
    baseline_path = repo_root / args.baseline
    output_path = repo_root / args.json_output

    packages = _load_packages(standards_path, repo_root)
    structure_issues = _validate_structure(packages)

    source_files: List[Path] = []
    for package in packages:
        source_files.extend(_iter_source_files(package.path))

    duplicate_count, duplicate_examples = _duplicate_blocks(source_files)
    complexity = _python_complexity(source_files)

    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    limits = baseline["limits"]

    failures: List[str] = []
    if structure_issues:
        failures.extend(structure_issues)
    if duplicate_count > limits["max_duplicate_blocks"]:
        failures.append(
            "duplicate_blocks=%d exceeds limit=%d"
            % (duplicate_count, limits["max_duplicate_blocks"])
        )
    if complexity["max_function_complexity"] > limits["max_function_complexity"]:
        failures.append(
            "max_function_complexity=%d exceeds limit=%d"
            % (complexity["max_function_complexity"], limits["max_function_complexity"])
        )
    if complexity["functions_over_30"] > limits["max_functions_over_30"]:
        failures.append(
            "functions_over_30=%d exceeds limit=%d"
            % (complexity["functions_over_30"], limits["max_functions_over_30"])
        )
    if complexity["functions_over_40"] > limits["max_functions_over_40"]:
        failures.append(
            "functions_over_40=%d exceeds limit=%d"
            % (complexity["functions_over_40"], limits["max_functions_over_40"])
        )

    result = {
        "status": "ok" if not failures else "failed",
        "limits": limits,
        "structure_issues": structure_issues,
        "duplicate_blocks": {
            "count": duplicate_count,
            "minimum_block_lines": MIN_DUPLICATE_BLOCK_LINES,
            "examples": duplicate_examples,
        },
        "python_complexity": complexity,
        "failures": failures,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    if failures:
        print("Package harmony validation failed.")
        print(json.dumps(result, indent=2))
        return 1

    print("Package harmony validation passed.")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
