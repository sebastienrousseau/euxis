# Euxis User Guide

Version: 4.5 | Last updated: 2026-01-30

## CLI Tools

### Core

| Command | Description |
|---------|-------------|
| `euxis <agent> <task> [provider]` | Route a task to any agent. Provider auto-selected via intelligence tiering if omitted. |
| `euxis-ui` | Interactive Mission Control TUI for deploying agents and browsing logs. |

### Quality & Certification

| Command | Description |
|---------|-------------|
| `euxis-lint` | Static analysis: registry integrity, protocol compliance, version sync. |
| `euxis-test-infra` | Infrastructure unit tests: argument validation, provider routing, space handling. |
| `euxis-certify` | Full certification pipeline (lint + infra tests + semantic verification). |
| `euxis-health` | Fleet health check: naming, hardening, orphans, headers, doc drift, providers. |

### Orchestration & Autonomy

| Command | Description |
|---------|-------------|
| `euxis-council "<topic>"` | 3-round adversarial debate (architect, edge-hunter, perf-optimizer). |
| `euxis-loop <agent> <task> <verify_cmd> [retries]` | Autonomous retry loop with reflexion and checkpoint support. |
| `euxis-dispatch` | Batch task dispatcher for parallel agent execution. |
| `euxis-kaizen` | 4-gate continuous self-improvement cycle. |
| `euxis-daemon [interval]` | Periodic kaizen loop with fail-safe halting. Default: 30 min. |

### Performance & Audit

| Command | Description |
|---------|-------------|
| `euxis-bench` | Performance benchmarking: health, lint, cortex recall, provider latency. |
| `euxis-audit-run` | Deep-dive audit: benchmarks, logic integrity, security probes, readiness report. |
| `euxis-gym <agent> <test_case> [provider]` | Agent evaluation and A/B testing against golden datasets. |

### Memory & Knowledge

| Command | Description |
|---------|-------------|
| `euxis-cortex remember "<fact>" [agent]` | Store a fact in the vector memory. |
| `euxis-cortex recall "<query>" [n]` | Semantic recall from the Cortex. |
| `euxis-cortex stats` | Show database statistics. |
| `euxis-cortex forget "<text>"` | Remove a specific memory entry. |

### Documentation & Governance

| Command | Description |
|---------|-------------|
| `euxis-sync-docs` | Librarian-powered doc sync with human-in-the-loop approval. |
| `euxis-optimize` | Prompt optimization and compression. |

### Infrastructure

| Command | Description |
|---------|-------------|
| `euxis-deploy` | Launch the enterprise fleet via Docker Compose. |
| `euxis-voice` | Voice interface: record, transcribe (Whisper), orchestrate, speak (Piper TTS). |

## Agent Fleet

### Core (3)

| Agent | Role |
|-------|------|
| `orchestrator` | Task decomposition, delegation, synthesis across the fleet |
| `architect` | Software architecture, patterns, ADRs, refactoring |
| `librarian` | Context architect, compliance custodian, memory optimizer |

### Fleet (19)

| Agent | Role |
|-------|------|
| `automation-engineer` | CI/CD pipelines, IaC, Docker, Terraform |
| `bug-fixer` | Debugging, root cause analysis, surgical fixes |
| `butler` | TTS-optimized summarization, spoken English output |
| `compliance-officer` | PII scanning, license auditing, GDPR/CCPA compliance |
| `data-steward` | Observability, telemetry, structured logging |
| `deep-researcher` | Iterative multi-pass research with cross-validation |
| `devrel-advocate` | Developer relations, tutorials, demos, conferences |
| `edge-hunter` | Security analysis, boundary testing, vulnerability assessment |
| `globalization-lead` | i18n, l10n, RTL support, Unicode validation |
| `growth-marketer` | SEO, AARRR funnel, CRO, GTM strategy |
| `legacy-maintainer` | Legacy code documentation, non-breaking upgrades |
| `perf-optimizer` | Latency, throughput, memory profiling |
| `product-manager` | Requirements, user stories, MoSCoW prioritization |
| `release-manager` | Changelogs, semantic versioning, release coordination |
| `reviewer` | Quality gate, output validation, completeness checking |
| `social-manager` | Platform-native content, calendars, community engagement |
| `tech-writer` | Documentation, tutorials, API reference, glossary |
| `unit-tester` | Test coverage, reliability, regression prevention |
| `ux-sentinel` | Accessibility (WCAG 2.1 AA), design system, responsive testing |

## Intelligence Tiering

When no provider is specified, agents are auto-routed:

| Tier | Agents | Provider |
|------|--------|----------|
| Strategic | orchestrator, architect, product-manager, reviewer | `claude` |
| Research | deep-researcher | `gemini` |
| Utility | butler, librarian | `ollama` |
| Coding | bug-fixer, legacy-maintainer | `opencode` |
| Default | all others | `claude` |

An explicit provider argument always overrides tiering.

## Quick Start

```bash
# Run an agent
euxis architect "Review the auth module"

# Check fleet health
euxis-health

# Full certification
euxis-certify

# Performance benchmark
euxis-bench

# Deep audit
euxis-audit-run

# Sync documentation (with approval)
euxis-sync-docs
```
