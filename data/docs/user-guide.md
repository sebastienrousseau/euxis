# Euxis User Guide

**Enterprise Unified eXecution Intelligence System**

version 0.0.4

For the complete user guide with CLI reference, agent fleet details, and provider matrix, see [guides/user-guide.md](guides/user-guide.md).

## Quick Start

```bash
# Verify a repository
euxis check .

# Fast triage (flash mode)
euxis triage .

# Deep review
euxis review .

# Compare triage vs standard
euxis compare .

# View metrics and drift
euxis stats --last 5

# Environment diagnostics
euxis doctor
```

## Core Commands

| Command | Description |
|---------|-------------|
| `euxis check [target]` | Verify a repository or target (standard mode) |
| `euxis triage [target]` | Fast bounded triage scan (flash mode) |
| `euxis review [target]` | Deep verification (standard/forensic) |
| `euxis compare <target>` | Compare triage vs deep verification |
| `euxis stats` | Validation metrics and drift history |
| `euxis policy <sub>` | Policy inspection and enforcement |
| `euxis doctor` | Environment diagnostics |

## Advanced Commands

```bash
# Run a specific playbook
euxis playbook verify-everything .

# Run an individual agent
euxis architect "Review the auth module"

# Multi-agent combo
euxis combo run envision "Design a notification system"

# Launch desktop GUI
euxis-etx
```

## Documentation Index

| Guide | Description |
|-------|-------------|
| [CLI Reference](reference/cli-reference.md) | Complete CLI command reference |
| [User Guide](guides/user-guide.md) | Full CLI reference, agent fleet, provider matrix |
| [UI Guide](guides/ui-guide.md) | Terminal UI usage and navigation |
| [API Architecture](api-architecture-design.md) | API design and architecture reference |
| [Core Concepts](essentials/core-concepts/) | Branding, protocols, and conventions |

---

Designed by Sebastien Rousseau — https://sebastienrousseau.com
Engineered with Euxis — Enterprise Unified Execution Intelligence System — https://euxis.co
