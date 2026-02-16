# Agents

Agent prompts, registries, and squads for the Euxis fleet.

## Purpose
Agents is the source of truth for the fleet: prompts, protocols, squads, and registries. It defines who the agents are and how they are composed into squads and combos.

## Structure
- `prompts/` — Agent prompts and shared protocols
- `registry.json` — JSON registry (fallback)
- `registry.db` — SQLite registry (primary)
- `squads.json` — Squad and combo definitions

## Dependencies
- `core/` — shared validation and prompt tooling
- `config/` — capabilities registry and policy configs

## Usage
```bash
# Inspect registry
jq '.agents | length' agents/registry.json
```

## Development
```bash
# Validate registry
python3 -m json.tool agents/registry.json
```

## Testing
```bash
# Registry tests
bash tests/lib/test_registry.sh
```

## API / Exports
Agents are referenced by ID in `agents/registry.json` and loaded via `core/lib/agents.sh`.
