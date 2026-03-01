# euxis-bridge

OpenClaw and ClawHub interoperability package for the Euxis runtime.

## What It Does

- Imports OpenClaw skill manifests from `SKILL.md` and `openclaw.json`.
- Normalizes imported skill metadata into Euxis bridge records.
- Provides policy-bounded Node execution wrappers for JavaScript skills.
- Exposes a CLI for import/export workflows.

## Quick Start

```bash
python -m euxis_bridge.cli import-clawhub --source ~/.openclaw/skills --output /tmp/euxis-bridge-registry.json
```

## Security Notes

- Node execution is bounded by timeout.
- Command environment can be allowlisted.
- Unknown skills are imported as metadata only until mapped/approved by policy.

## License

AGPL-3.0-or-later
