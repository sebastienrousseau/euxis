# Euxis

**Multi-repo workspace for the Euxis Agent Framework**

[![License][license-badge]][license-url]
[![Version][version-badge]][releases-url]

Version 0.1.0

---

## Repositories

This workspace contains the following packages:

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

## Author

**Sebastien Rousseau** — [sebastienrousseau.com](https://sebastienrousseau.com)

---

*Euxis v0.1.0 — Part of the [Euxis Agent Framework](https://euxis.co)*

[license-badge]: https://img.shields.io/badge/license-MIT-blue.svg
[license-url]: LICENSE
[version-badge]: https://img.shields.io/badge/version-0.1.0-green.svg
[releases-url]: https://github.com/euxis/euxis/releases
