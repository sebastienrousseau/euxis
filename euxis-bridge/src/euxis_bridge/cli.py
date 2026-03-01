"""CLI for Euxis bridge import workflows."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import json
from pathlib import Path

from euxis_bridge.core.importer import ClawHubImporter
from euxis_bridge.core.static_analysis import analyze_skill_directory
from euxis_bridge.core.admission import AdmissionPipeline
from euxis_bridge.core.reputation import ReputationStore


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Euxis bridge CLI")
    sub = parser.add_subparsers(dest="command", required=True)

    import_cmd = sub.add_parser("import-clawhub", help="Import OpenClaw/ClawHub skills")
    import_cmd.add_argument("--source", required=True, help="Path to OpenClaw skills root")
    import_cmd.add_argument("--output", required=True, help="Path to output registry JSON")

    verify_cmd = sub.add_parser("verify-skill", help="Run admission pipeline on a skill")
    verify_cmd.add_argument("--skill-dir", required=True, help="Path to skill directory")
    verify_cmd.add_argument("--public-key", default=None, help="Path to Ed25519 public key")
    verify_cmd.add_argument("--strict", action="store_true", help="Require signature and reputation checks")

    analyze_cmd = sub.add_parser("analyze-skill", help="Run static analysis on a skill")
    analyze_cmd.add_argument("--skill-dir", required=True, help="Path to skill directory")
    analyze_cmd.add_argument("--format", choices=["json", "text"], default="text", help="Output format")

    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)

    if args.command == "import-clawhub":
        importer = ClawHubImporter(Path(args.source).expanduser())
        report = importer.export_registry(Path(args.output).expanduser())
        print(json.dumps(asdict(report), sort_keys=True))
        return 0

    if args.command == "verify-skill":
        skill_dir = Path(args.skill_dir).expanduser()
        pipeline = AdmissionPipeline(
            public_key_path=args.public_key,
            require_signature=args.strict,
            require_reputation=False,
        )
        result = pipeline.evaluate(skill_dir=skill_dir)
        print(json.dumps(asdict(result), sort_keys=True))
        return 0 if result.admitted else 1

    if args.command == "analyze-skill":
        skill_dir = Path(args.skill_dir).expanduser()
        report = analyze_skill_directory(skill_dir)

        if args.format == "json":
            print(json.dumps(asdict(report), sort_keys=True))
        else:
            print(f"Scanned files: {report.scanned_files}")
            print(f"Passed: {report.passed}")
            print(f"Findings: {len(report.findings)}")
            for finding in report.findings:
                print(f"  [{finding.severity}] {finding.location}: {finding.description}")

        return 0 if report.passed else 1

    return 1


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
