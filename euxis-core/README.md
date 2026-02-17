# euxis-core

Shared core logic and shell libraries for the Euxis Agent Framework.

## Overview

`euxis-core` provides the foundational components for the Euxis multi-agent system:

- **Shell Libraries** (`lib/`): Reusable shell functions for agents, providers, sessions, and validation
- **Core Utilities**: Common patterns and helpers used across the Euxis ecosystem

## Installation

```bash
pip install euxis-core
```

For development:

```bash
pip install -e .[dev]
```

## Shell Libraries

The `lib/` directory contains shell libraries that can be sourced:

| Library | Purpose |
|---------|---------|
| `common.sh` | Foundation utilities (logging, paths) |
| `session.sh` | Session management |
| `template.sh` | Prompt templating |
| `validation.sh` | Input validation and security checks |
| `providers.sh` | LLM provider integration |
| `agents.sh` | Agent registry and dispatch |
| `memory.sh` | Cortex memory operations |
| `dispatch.sh` | Task orchestration |

## Testing

```bash
# Run Python tests
pytest

# Run shell tests (requires bats)
bats tests/bats/
```

## License

MIT License - see [LICENSE](LICENSE)

---
*Part of the [Euxis Agent Framework](https://euxis.co)*
