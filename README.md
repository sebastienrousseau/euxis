# Euxis

**Enterprise Unified eXecution Intelligence System**

[![License][license-badge]][license-url]
[![Version][version-badge]][version-url]

---

## Repositories

Euxis is a modular multi-package workspace. Each package serves a distinct function within the agent framework:

| Package | Description | Version |
|---------|-------------|---------|
| [euxis-core](./euxis-core) | Shared core logic and shell libraries | 0.1.0 |
| [euxis-cli](./euxis-cli) | CLI entry points and orchestration | 0.1.0 |
| [euxis-gateway](./euxis-gateway) | WebSocket control plane and API | 0.1.0 |
| [euxis-adapters](./euxis-adapters) | Channel adapters (Slack, Telegram) | 0.1.0 |
| [euxis-tui](./euxis-tui) | Textual UI application | 0.1.0 |
| [euxis-metrics](./euxis-metrics) | Observability and metrics | 0.1.0 |
| [euxis-security](./euxis-security) | Security policy defaults | 0.1.0 |
| [euxis-crypto-lib](./euxis-crypto-lib) | Python cryptography utilities | 0.1.0 |
| [euxis-crypto-packages](./euxis-crypto-packages) | TypeScript crypto packages | 0.1.0 |
| [euxis-docs](./euxis-docs) | Documentation and ADRs | 0.1.0 |

## Installation

### From PyPI

```bash
pip install euxis-core euxis-cli euxis-tui
```

### From Source

```bash
# Clone this workspace
git clone https://github.com/euxis/euxis.git
cd euxis

# Install packages in development mode
pip install -e euxis-core
pip install -e euxis-cli
pip install -e euxis-tui
```

## Usage

### CLI Commands

```bash
# Run an agent
euxis architect "Design a REST API for user management"

# Run health check
euxis health

# Verify release readiness
euxis verify --all

# Launch TUI
euxis ui
```

### Quick Start

See individual package READMEs for detailed usage instructions.

## Current Status (February 18, 2026)

Euxis is usable, but early users should expect active stabilization in some test
surfaces while provider workflows are already in strong shape.

- Provider shell unit suite: `53/53` passing (`euxis-core/tests/bats/lib/providers.bats`)
- Provider helper coverage (function-level shell gate): `100%` (`63/63`)
- Provider suite local runtime: `~3.94s` on the current dev machine
- Adjacent shell suites still being stabilized:
  - `common.bats`: `29/30` passing
  - `session.bats`: `30/34` passing
- Python coverage policy remains `fail_under = 95` in `pyproject.toml`

For full context, caveats, and a verification command list, see:
- `euxis-docs/docs/reports/initial-user-expectations-2026-02-18.md`
- `euxis-docs/docs/index.rst`

## Dependency Graph

```
euxis-core (no dependencies)
    │
    ├── euxis-cli
    ├── euxis-tui
    ├── euxis-metrics
    │
    └── euxis-security
            │
            └── euxis-gateway
                    │
                    └── euxis-adapters

euxis-crypto-lib (standalone)
euxis-crypto-packages (standalone, TypeScript)
euxis-docs (standalone)
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

MIT License - see [LICENSE](LICENSE)

---

---

Version 0.1.0

Designed by Sebastien Rousseau — https://sebastienrousseau.com
Engineered with Euxis — Enterprise Unified Execution Intelligence System — https://euxis.co
Euxis v0.1.0

[license-badge]: https://img.shields.io/badge/license-MIT-blue.svg
[license-url]: LICENSE
[version-badge]: https://img.shields.io/badge/version-0.1.0-green.svg
[version-url]: https://github.com/sebastienrousseau/euxis/releases/tag/v0.1.0
