# Euxis User Guide

Euxis runs cryptographically-signed code audits at native speed using a multi-agent fleet routed across multiple LLM providers. This page is the one-screen overview of the CLI surface; the deep reference (full flag listings, agent fleet matrix, provider routing rules) lives at [`guides/user-guide.md`](guides/user-guide.md).

**Version:** v0.1.2

## Quick Start

```bash
# Verify a repository (standard mode)
euxis check .

# Fast bounded triage (flash mode)
euxis triage .

# Deep verification with cross-checking
euxis review .

# Run a triage vs deep diff to see what flash missed
euxis compare .

# Inspect recent validation metrics and confidence drift
euxis stats --last 5

# Environment diagnostics — call this first when anything misbehaves
euxis doctor
```

## Core Commands

Each command takes an optional target path; absent it, the current directory is implied.

| Command | What it does |
|---------|--------------|
| `euxis check [target]` | Standard verification — runs the full audit pipeline against the target |
| `euxis triage [target]` | Bounded flash scan — fast, lower-coverage triage suitable for pre-commit hooks |
| `euxis review [target]` | Deep verification — the slowest, highest-confidence mode (standard or forensic) |
| `euxis compare <target>` | Runs both triage and standard and reports which findings differ |
| `euxis stats` | Validation metrics + confidence-drift history across recent runs |
| `euxis policy <sub>` | Inspect and enforce policy rules (allowlist, severity floor, scope) |
| `euxis doctor` | Probe the environment — toolchain, model providers, credentials, cache health |

## Advanced Commands

```bash
# Run a playbook by name (composes multiple agents in a fixed sequence)
euxis playbook verify-everything .

# Invoke a single named agent directly
euxis architect "Review the auth module"

# Multi-agent combo — fan-out + reconciliation
euxis combo run envision "Design a notification system"

# Launch the Qt6 desktop GUI
euxis-etx
```

## Documentation Index

| Page | What it covers |
|------|---------------|
| [CLI Reference](reference/cli-reference.md) | Full flag listing for every CLI command |
| [User Guide](guides/user-guide.md) | Agent fleet, provider matrix, full configuration reference |
| [UI Guide](guides/ui-guide.md) | Terminal UI and desktop GUI usage |
| [API Architecture](api-architecture-design.md) | API-side design rationale and surface map |
| [Core Concepts](essentials/core-concepts/) | Branding, protocols, and naming conventions |

---

Designed by Sebastien Rousseau — https://sebastienrousseau.com
Engineered with Euxis — Enterprise Unified Execution Intelligence System — https://euxis.co
