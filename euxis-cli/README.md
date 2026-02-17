# euxis-cli

CLI entry points and orchestration commands for the Euxis Agent Framework.

## Overview

`euxis-cli` provides the command-line tools for interacting with the Euxis multi-agent system:

- **Agent Invocation**: Run agents directly from the command line
- **Orchestration**: Dispatch tasks, manage squads, run playbooks
- **Development Tools**: Linting, testing, certification, benchmarking
- **System Management**: Health checks, daemon control, bus operations

## Installation

```bash
pip install euxis-cli
```

For development:

```bash
pip install -e .[dev]
```

## CLI Commands

### Core Commands

| Command | Description |
|---------|-------------|
| `euxis` | Main entry point for agent invocation |
| `euxis-dispatch` | Task dispatch and orchestration |
| `euxis-squad` | Multi-agent squad management |
| `euxis-playbook` | Run automated playbooks |

### Development Commands

| Command | Description |
|---------|-------------|
| `euxis-lint` | Static analysis and linting |
| `euxis-certify` | Full certification pipeline |
| `euxis-test-infra` | Infrastructure tests |
| `euxis-bench` | Performance benchmarking |

### System Commands

| Command | Description |
|---------|-------------|
| `euxis-health` | System health check |
| `euxis-daemon` | Background daemon control |
| `euxis-bus` | Message bus operations |
| `euxis-cortex` | Memory/cortex management |

## Usage

```bash
# Run an agent
euxis architect "Design a REST API"

# Dispatch a task
euxis-dispatch --agent architect --task "Review code"

# Run certification
euxis-certify

# Check system health
euxis-health
```

## Testing

```bash
pytest
```

## License

MIT License - see [LICENSE](LICENSE)

---
*Part of the [Euxis Agent Framework](https://euxis.co)*
