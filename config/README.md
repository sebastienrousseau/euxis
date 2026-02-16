# Config

Configuration schemas, defaults, and templates for Euxis.

## Purpose
Owns configuration files and schemas that define runtime behavior for gateway, agents, and tooling.

## Structure
- `gateway.json` — Gateway defaults
- `capabilities.json` — Capability registry
- `codex/` — Codex config
- `playbooks/` — Playbook definitions

## Dependencies
- `core/` — validation utilities

## Usage
Configurations are loaded by runtime modules using `EUXIS_HOME/config`.

## Development
```bash
jq '.' config/gateway.json
```

## Testing
Config validation runs during `euxis-certify`.

## API / Exports
Schemas are referenced in `docs/reference/`.
