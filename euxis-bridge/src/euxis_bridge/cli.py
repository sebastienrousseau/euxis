"""CLI for Euxis bridge import workflows."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import json
from pathlib import Path

from euxis_bridge.core.importer import ClawHubImporter


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Euxis bridge CLI")
    sub = parser.add_subparsers(dest="command", required=True)

    import_cmd = sub.add_parser("import-clawhub", help="Import OpenClaw/ClawHub skills")
    import_cmd.add_argument("--source", required=True, help="Path to OpenClaw skills root")
    import_cmd.add_argument("--output", required=True, help="Path to output registry JSON")

    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)

    importer = ClawHubImporter(Path(args.source).expanduser())
    report = importer.export_registry(Path(args.output).expanduser())
    print(json.dumps(asdict(report), sort_keys=True))
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
