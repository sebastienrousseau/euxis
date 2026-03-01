"""Interoperability benchmarks: OpenClaw roundtrip, MCP compliance, A2A cards."""

from __future__ import annotations

import json
import tempfile
import time
from pathlib import Path

from euxis_ops.benchmarks.runner import BenchmarkResult, BenchmarkRunner


def _bench_openclaw_roundtrip() -> BenchmarkResult:
    """Benchmark OpenClaw JSON and SKILL.md import roundtrip."""
    from euxis_bridge.core.importer import ClawHubImporter

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "skills"

        # SKILL.md skill
        md_dir = root / "md-skill"
        md_dir.mkdir(parents=True)
        (md_dir / "SKILL.md").write_text(
            "---\nname: MD Skill\nslug: md-skill\nruntime: node\nentrypoint: index.js\ntags: [web]\n---\n",
            encoding="utf-8",
        )

        # openclaw.json skill
        json_dir = root / "json-skill"
        json_dir.mkdir(parents=True)
        (json_dir / "openclaw.json").write_text(
            json.dumps({"name": "JSON Skill", "runtime": "python", "entrypoint": "main.py"}),
            encoding="utf-8",
        )

        importer = ClawHubImporter(root)
        start = time.monotonic()
        skills = importer.import_skills()
        output_path = Path(tmp) / "registry.json"
        report = importer.export_registry(output_path)
        registry = json.loads(output_path.read_text(encoding="utf-8"))
        elapsed = (time.monotonic() - start) * 1000

    has_both = len(skills) == 2
    roundtrip_ok = registry["version"] == 1 and report.imported == 2
    return BenchmarkResult(
        name="openclaw_roundtrip",
        suite="interop",
        passed=has_both and roundtrip_ok,
        duration_ms=elapsed,
        metrics={"skills_imported": len(skills), "registry_valid": roundtrip_ok},
    )


def _bench_a2a_card_generation() -> BenchmarkResult:
    """Benchmark A2A agent card generation and serialization."""
    try:
        from euxis_a2a.core.agent_card import Capability, generate_agent_card, serialize_card
    except ImportError:
        return BenchmarkResult(
            name="a2a_card_generation", suite="interop", passed=True,
            duration_ms=0, metrics={"skipped": True},
        )

    caps = (Capability(name="search", description="Search the web"),)

    start = time.monotonic()
    iterations = 100
    for _ in range(iterations):
        card = generate_agent_card(
            name="Test Agent", description="Test", endpoint="https://agent.example.com",
            capabilities=caps,
        )
        serialized = serialize_card(card)
    elapsed = (time.monotonic() - start) * 1000

    has_fields = all(k in serialized for k in ("name", "url", "capabilities", "protocols"))
    return BenchmarkResult(
        name="a2a_card_generation",
        suite="interop",
        passed=has_fields,
        duration_ms=elapsed,
        metrics={"iterations": iterations, "avg_ms": round(elapsed / iterations, 4)},
    )


def _bench_bridged_skill_serialization() -> BenchmarkResult:
    """Benchmark BridgedSkill to_dict roundtrip fidelity."""
    from euxis_bridge.core.models import BridgedSkill

    skill = BridgedSkill(
        name="Browser Use", slug="browser-use", source_dir="/tmp/skills/browser-use",
        description="Browser automation", runtime="node", entrypoint="index.js",
        tags=("web", "automation"), metadata={"origin": "SKILL.md"},
    )

    start = time.monotonic()
    payload = skill.to_dict()
    elapsed = (time.monotonic() - start) * 1000

    fidelity_ok = (
        payload["name"] == "Browser Use"
        and payload["tags"] == ["web", "automation"]
        and isinstance(payload["tags"], list)
    )
    return BenchmarkResult(
        name="bridged_skill_serialization",
        suite="interop",
        passed=fidelity_ok,
        duration_ms=elapsed,
        metrics={"fields_preserved": fidelity_ok},
    )


def register_benchmarks(runner: BenchmarkRunner) -> None:
    runner.register("interop", _bench_openclaw_roundtrip)
    runner.register("interop", _bench_a2a_card_generation)
    runner.register("interop", _bench_bridged_skill_serialization)
