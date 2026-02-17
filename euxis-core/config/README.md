# Config

Configuration schemas, defaults, and templates for Euxis.

## Purpose
Owns configuration files and schemas that define runtime behavior for gateway, agents, and tooling.

## Structure
- `capabilities.json` — Capability registry
- `codex/` — Codex config
- `playbooks/` — Playbook definitions

## Dependencies
- `core/` — validation utilities

## Usage
Configurations are loaded by runtime modules using `EUXIS_HOME/config`. Gateway policy defaults live in `security/gateway.json`.

## Development
```bash
jq '.' security/gateway.json
```

## Testing
Config validation runs during `euxis-certify`.

## API / Exports
Schemas are referenced in `docs/reference/`.
