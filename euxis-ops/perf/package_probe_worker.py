#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Execute a lightweight package probe and emit resource metrics as JSON."""

from __future__ import annotations

import argparse
import json
import os
import resource
import time
from pathlib import Path


def _count_text_lines(path: Path, pattern: str) -> int:
    total = 0
    for file_path in path.rglob(pattern):
        if file_path.is_file():
            try:
                total += len(file_path.read_text(encoding="utf-8", errors="ignore").splitlines())
            except OSError:
                continue
    return total


def _probe_python(path: Path) -> dict[str, int]:
    pyproject = path / "pyproject.toml"
    lines = _count_text_lines(path, "*.py")
    payload = pyproject.read_text(encoding="utf-8", errors="ignore") if pyproject.exists() else ""
    return {"python_lines": lines, "pyproject_bytes": len(payload.encode("utf-8"))}


def _probe_rust(path: Path) -> dict[str, int]:
    cargo = path / "Cargo.toml"
    payload = cargo.read_text(encoding="utf-8", errors="ignore") if cargo.exists() else ""
    rs_lines = _count_text_lines(path, "*.rs")
    return {"rust_lines": rs_lines, "cargo_toml_bytes": len(payload.encode("utf-8"))}


def _probe_node(path: Path) -> dict[str, int]:
    package_json = path / "package.json"
    payload = package_json.read_text(encoding="utf-8", errors="ignore") if package_json.exists() else "{}"
    obj = json.loads(payload)
    keys = len(obj.keys()) if isinstance(obj, dict) else 0
    return {"package_json_keys": keys, "package_json_bytes": len(payload.encode("utf-8"))}


def _probe_runtime_data(path: Path) -> dict[str, int]:
    metrics = path / "data" / "perf" / "metrics.jsonl"
    count = 0
    bytes_read = 0
    if metrics.exists():
        with metrics.open("r", encoding="utf-8", errors="ignore") as handle:
            for line in handle:
                if not line.strip():
                    continue
                bytes_read += len(line.encode("utf-8"))
                count += 1
                if count >= 500:
                    break
    return {"metrics_samples_read": count, "metrics_bytes_read": bytes_read}


def _probe_ops(path: Path) -> dict[str, int]:
    sh_files = len(list(path.rglob("*.sh")))
    py_files = len(list(path.rglob("*.py")))
    return {"shell_files": sh_files, "python_files": py_files}


def _probe_docs(path: Path) -> dict[str, int]:
    md_files = len(list(path.rglob("*.md")))
    rst_files = len(list(path.rglob("*.rst")))
    return {"markdown_files": md_files, "rst_files": rst_files}


def _run_probe(kind: str, path: Path) -> dict[str, int]:
    if kind in {"python", "python-rust"}:
        return _probe_python(path)
    if kind == "rust":
        return _probe_rust(path)
    if kind == "node":
        return _probe_node(path)
    if kind == "runtime-data":
        return _probe_runtime_data(path)
    if kind == "ops":
        return _probe_ops(path)
    if kind == "docs":
        return _probe_docs(path)
    return {"unknown_probe": 1}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-path", required=True)
    parser.add_argument("--kind", required=True)
    args = parser.parse_args()

    package_path = Path(args.package_path).resolve()
    if not package_path.exists():
        print(json.dumps({"error": f"package path missing: {package_path}"}))
        return 1

    start_wall = time.perf_counter()
    start_cpu = time.process_time()
    probe = _run_probe(args.kind, package_path)
    elapsed_ms = (time.perf_counter() - start_wall) * 1000.0
    cpu_ms = (time.process_time() - start_cpu) * 1000.0
    max_rss_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    cpu_pct = (cpu_ms / elapsed_ms) * 100.0 if elapsed_ms > 0 else 0.0

    payload = {
        "pid": os.getpid(),
        "kind": args.kind,
        "wall_ms": round(elapsed_ms, 3),
        "cpu_ms": round(cpu_ms, 3),
        "cpu_pct": round(cpu_pct, 3),
        "max_rss_kb": int(max_rss_kb),
        "probe": probe,
    }
    print(json.dumps(payload))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
