# (c) 2026 Euxis Fleet. All rights reserved.
# Euxis CLI Reference

Complete reference for all Euxis command-line tools.

## Table of Contents

### Core System
- [euxis](#euxis) - Main Euxis CLI entry point
- [euxis-daemon](#euxis-daemon) - Background daemon management
- [euxis-gateway](#euxis-gateway) - Gateway control plane
- [euxis-health](#euxis-health) - System health monitoring
- [euxis-verify](#euxis-verify) - Single verification runner
- [euxis-verify-all](#euxis-verify-all) - Comprehensive verification

### Memory & Knowledge
- [euxis-cortex](#euxis-cortex) - Semantic memory system
- [euxis-graph](#euxis-graph) - Knowledge graph operations
- [euxis-codex](#euxis-codex) - Code knowledge indexing

### Agent Orchestration
- [euxis-dispatch](#euxis-dispatch) - Multi-agent task coordination
- [euxis-council](#euxis-council) - Agent council management
- [euxis-synthesize](#euxis-synthesize) - Dynamic agent synthesis
- [euxis-squad](#euxis-squad) - Squad-based agent groups

### Development & Quality
- [euxis-lint](#euxis-lint) - Code quality checks
- [euxis-certify](#euxis-certify) - Release certification
- [euxis-audit-run](#euxis-audit-run) - Audit execution
- [euxis-license-check](#euxis-license-check) - License compliance
- [euxis-test-infra](#euxis-test-infra) - Test infrastructure
- [euxis-cross-platform-verify](#euxis-cross-platform-verify) - Cross-platform validation

### Automation & Optimization
- [euxis-bench](#euxis-bench) - Performance benchmarking
- [euxis-optimize](#euxis-optimize) - Performance optimization
- [euxis-kaizen](#euxis-kaizen) - Continuous improvement
- [euxis-polish](#euxis-polish) - Code polish and cleanup
- [euxis-combo](#euxis-combo) - Combo action sequences

### Communication & Integration
- [euxis-bus](#euxis-bus) - Message bus operations
- [euxis-playbook](#euxis-playbook) - Playbook execution
- [euxis-loop](#euxis-loop) - Event loop management
- [euxis-hooks](#euxis-hooks) - Git hooks management
- [euxis-git-guard](#euxis-git-guard) - Git operation protection

### User Interface
- [euxis-ui](#euxis-ui) - User interface tools
- [euxis-voice](#euxis-voice) - Voice interface
- [euxis-slash](#euxis-slash) - Slash command processor

### Utilities
- [euxis-deploy](#euxis-deploy) - Deployment operations
- [euxis-sync-docs](#euxis-sync-docs) - Documentation synchronization
- [euxis-gym](#euxis-gym) - Training and simulation environment

---

## Core System

### euxis

**Synopsis:** `euxis [command] [options]`

**Description:**
Main entry point for the Euxis system. Provides unified access to all Euxis functionality.

**Options:**
- Standard CLI patterns supported

**Examples:**
```bash
euxis --version
euxis help
```

**See Also:** All other euxis-* commands

---

### euxis-daemon

**Synopsis:** `euxis-daemon [start|stop|status]`

**Description:**
Manages the Euxis background daemon process for continuous operation and monitoring.

**Options:**
- `start` - Start the daemon
- `stop` - Stop the daemon
- `status` - Check daemon status

**Examples:**
```bash
euxis-daemon start
euxis-daemon status
```

**See Also:** [euxis-health](#euxis-health)

---

### euxis-gateway

**Synopsis:** `euxis-gateway <command> [options]`

**Description:**
Runs and manages the Gateway control plane for channels, sessions, and WebSocket clients.

**Commands:**
- `run` - Start the Gateway
- `status` - Show Gateway status
- `config` - Print resolved Gateway configuration
- `sessions` - List active sessions

**Options:**
- `--bind <addr>` - Bind address (default: `127.0.0.1`)
- `--port <port>` - Bind port (default: `18789`)
- `--config <path>` - Path to gateway config (default: `~/.euxis/security/gateway.json`)
- `--auth-mode <token|password>` - Override auth mode

**Examples:**
```bash
euxis-gateway run
euxis-gateway run --bind 0.0.0.0 --port 18789
euxis-gateway status
euxis-gateway config
```

**See Also:** [Gateway Protocol](gateway.md), [Gateway Config](gateway-config.md)

---
### euxis-health

**Synopsis:** `euxis-health [check|monitor|report]`

**Description:**
Comprehensive system health monitoring and reporting for Euxis infrastructure.

**Options:**
- `check` - Run health checks
- `monitor` - Continuous monitoring
- `report` - Generate health report

**Examples:**
```bash
euxis-health check
euxis-health monitor --interval 30
```

**See Also:** [euxis-daemon](#euxis-daemon), [euxis-verify](#euxis-verify)

---

### euxis-verify

**Synopsis:** `euxis-verify [target]`

**Description:**
Runs verification checks for specific targets or components.

**Examples:**
```bash
euxis-verify security
euxis-verify performance
```

**See Also:** [euxis-verify-all](#euxis-verify-all)

---

### euxis-verify-all

**Synopsis:** `euxis-verify-all [--parallel] [--fail-fast]`

**Description:**
Comprehensive verification of all system components and quality gates.

**Options:**
- `--parallel` - Run checks in parallel
- `--fail-fast` - Stop on first failure

**Examples:**
```bash
euxis-verify-all
euxis-verify-all --parallel --fail-fast
```

**See Also:** [euxis-verify](#euxis-verify), [euxis-certify](#euxis-certify)

---

## Memory & Knowledge

### euxis-cortex

**Synopsis:** `euxis-cortex <command> [args]`

**Description:**
Cross-platform semantic memory system using vector and graph hybrid storage. Manages episodic, semantic, and procedural knowledge.

**Commands:**
- `remember 'fact' [source] [--type TYPE]` - Store a memory
- `recall 'query' [n] [--type TYPE] [--hybrid]` - Retrieve memories
- `relate 'query' <entity> --relation <type>` - Create relationships
- `stats` - Show memory statistics
- `forget 'text'` - Remove specific memory

**Memory Types:**
- `episodic` - Specific events, outcomes, timestamps (default)
- `semantic` - General facts, rules, relationships
- `procedural` - Reusable workflows, patterns, contraindications

**Graph Relations:**
- `related_to`, `produced_by`, `depends_on`, `contradicts`, `references`, `part_of`

**Options:**
- `--hybrid` - Combine vector similarity with graph proximity for recall

**Examples:**
```bash
euxis-cortex remember 'Deployed v0.0.8 to staging, all tests passed' 'gatekeeper' --type episodic
euxis-cortex recall 'auth token' 5 --hybrid
euxis-cortex relate 'JWT implementation' auth-module --relation part_of
```

**See Also:** [euxis-graph](#euxis-graph), [euxis-codex](#euxis-codex)

---

### euxis-graph

**Synopsis:** `euxis-graph <command> [args]`

**Description:**
Knowledge graph operations for entity relationships and semantic connections.

**Commands:**
- Various graph operations for entities and relationships

**Examples:**
```bash
euxis-graph query "auth-module"
euxis-graph add-edge source target relationship
```

**See Also:** [euxis-cortex](#euxis-cortex)

---

### euxis-codex

**Synopsis:** `euxis-codex <command> [args]`

**Description:**
Code knowledge indexing and retrieval system for codebase understanding.

**Examples:**
```bash
euxis-codex index
euxis-codex search "authentication"
```

**See Also:** [euxis-cortex](#euxis-cortex), [euxis-graph](#euxis-graph)

---

## Agent Orchestration

### euxis-dispatch

**Synopsis:** `euxis-dispatch <manifest.json> [--mode MODE]`

**Description:**
Multi-agent task coordination using dispatch manifests. Orchestrates parallel and sequential agent workflows.

**Modes:**
- `hierarchical` - Central coordination, all agents report to orchestrator (default)
- `mesh` - Agents with dispatch authority coordinate sub-workflows
- `federated` - Agents operate autonomously across project boundaries

**Examples:**
```bash
euxis-dispatch manifest.json
euxis-dispatch release-manifest.json --mode mesh
```

**Manifest Generation:**
```bash
euxis architect 'Analyze repo and output MISSION MANIFEST.' claude > manifest.json
```

**See Also:** [euxis-council](#euxis-council), [euxis-synthesize](#euxis-synthesize)

---

### euxis-council

**Synopsis:** `euxis-council <command> [args]`

**Description:**
Agent council management for collaborative decision-making and consensus building.

**Examples:**
```bash
euxis-council convene security-review
euxis-council vote proposal-123
```

**See Also:** [euxis-dispatch](#euxis-dispatch), [euxis-squad](#euxis-squad)

---

### euxis-synthesize

**Synopsis:** `euxis-synthesize <command> [args]`

**Description:**
Dynamic agent synthesis for creating specialized, temporary agents with scoped capabilities.

**Commands:**
- `compose` - Create a new synthesized agent
- `dissolve` - Remove a synthesized agent

**Examples:**
```bash
euxis-synthesize compose custom-validator --capabilities validation,testing
euxis-synthesize dissolve custom-validator
```

**See Also:** [euxis-dispatch](#euxis-dispatch)

---

### euxis-squad

**Synopsis:** `euxis-squad <command> [args]`

**Description:**
Squad-based agent group management for coordinated team operations.

**Examples:**
```bash
euxis-squad create security-team
euxis-squad assign pentester security-team
```

**See Also:** [euxis-council](#euxis-council), [euxis-dispatch](#euxis-dispatch)

---

## Development & Quality

### euxis-lint

**Synopsis:** `euxis-lint [target] [--fix] [--check TYPE]`

**Description:**
Comprehensive code quality checks including syntax, style, security, and best practices.

**Options:**
- `--fix` - Automatically fix issues where possible
- `--check TYPE` - Run specific check type

**Examples:**
```bash
euxis-lint
euxis-lint src/ --fix
euxis-lint --check security
```

**See Also:** [euxis-certify](#euxis-certify), [euxis-audit-run](#euxis-audit-run)

---

### euxis-certify

**Synopsis:** `euxis-certify [--stage STAGE] [--profile PROFILE]`

**Description:**
Release certification and quality gate validation. Ensures code meets production readiness standards.

**Options:**
- `--stage` - Certification stage
- `--profile` - Certification profile

**Examples:**
```bash
euxis-certify
euxis-certify --stage production --profile security
```

**See Also:** [euxis-verify-all](#euxis-verify-all), [euxis-lint](#euxis-lint)

---

### euxis-audit-run

**Synopsis:** `euxis-audit-run [audit-type] [target]`

**Description:**
Executes security, compliance, and quality audits on the codebase.

**Examples:**
```bash
euxis-audit-run security
euxis-audit-run compliance src/
```

**See Also:** [euxis-certify](#euxis-certify), [euxis-license-check](#euxis-license-check)

---

### euxis-license-check

**Synopsis:** `euxis-license-check [--scan] [--report]`

**Description:**
License compliance verification for dependencies and source code.

**Options:**
- `--scan` - Scan for license issues
- `--report` - Generate compliance report

**Examples:**
```bash
euxis-license-check --scan
euxis-license-check --report
```

**See Also:** [euxis-audit-run](#euxis-audit-run)

---

### euxis-test-infra

**Synopsis:** `euxis-test-infra [command] [target]`

**Description:**
Test infrastructure management and validation. Coordinates test execution across multiple environments.

**Examples:**
```bash
euxis-test-infra run
euxis-test-infra validate integration
```

**See Also:** [euxis-verify](#euxis-verify), [euxis-cross-platform-verify](#euxis-cross-platform-verify)

---

### euxis-cross-platform-verify

**Synopsis:** `euxis-cross-platform-verify [--platform PLATFORM]`

**Description:**
Cross-platform compatibility verification for Darwin, Linux, and Windows environments.

**Options:**
- `--platform` - Target specific platform

**Examples:**
```bash
euxis-cross-platform-verify
euxis-cross-platform-verify --platform linux
```

**See Also:** [euxis-test-infra](#euxis-test-infra)

---

## Automation & Optimization

### euxis-bench

**Synopsis:** `euxis-bench [target] [--profile PROFILE]`

**Description:**
Performance benchmarking and profiling for system components.

**Options:**
- `--profile` - Specific benchmark profile

**Examples:**
```bash
euxis-bench
euxis-bench memory --profile intensive
```

**See Also:** [euxis-optimize](#euxis-optimize)

---

### euxis-optimize

**Synopsis:** `euxis-optimize [target] [options]`

**Description:**
Performance optimization recommendations and automatic improvements.

**Examples:**
```bash
euxis-optimize
euxis-optimize database-queries
```

**See Also:** [euxis-bench](#euxis-bench)

---

### euxis-kaizen

**Synopsis:** `euxis-kaizen [focus-area]`

**Description:**
Continuous improvement analysis and recommendations for the Euxis system.

**Examples:**
```bash
euxis-kaizen
euxis-kaizen workflow-efficiency
```

**See Also:** [euxis-optimize](#euxis-optimize)

---

### euxis-polish

**Synopsis:** `euxis-polish [target]`

**Description:**
Code polish and cleanup operations for improved readability and maintainability.

**Examples:**
```bash
euxis-polish
euxis-polish src/agents/
```

**See Also:** [euxis-lint](#euxis-lint)

---

### euxis-combo

**Synopsis:** `euxis-combo <sequence> [args]`

**Description:**
Executes predefined sequences of Euxis operations for common workflows.

**Examples:**
```bash
euxis-combo deploy-prep
euxis-combo full-audit
```

**See Also:** [euxis-playbook](#euxis-playbook)

---

## Communication & Integration

### euxis-bus

**Synopsis:** `euxis-bus <command> [args]`

**Description:**
Message bus operations for inter-agent communication and event coordination.

**Commands:**
- `publish` - Publish messages to topics
- `subscribe` - Subscribe to topic updates
- `list-topics` - Show available topics

**Examples:**
```bash
euxis-bus publish deployment-events "Release v1.0 completed"
euxis-bus subscribe security-alerts
```

**See Also:** [euxis-playbook](#euxis-playbook), [euxis-hooks](#euxis-hooks)

---

### euxis-playbook

**Synopsis:** `euxis-playbook <command> [playbook] [args]`

**Description:**
Playbook execution engine for standardized operational procedures and incident response.

**Commands:**
- `run` - Execute playbook
- `list` - List available playbooks
- `validate` - Validate playbook syntax

**Examples:**
```bash
euxis-playbook run incident-response
euxis-playbook list
euxis-playbook validate security-audit.yml
```

**See Also:** [euxis-bus](#euxis-bus), [euxis-loop](#euxis-loop)

---

### euxis-loop

**Synopsis:** `euxis-loop <command> [args]`

**Description:**
Event loop management for continuous processing and monitoring.

**Examples:**
```bash
euxis-loop start monitoring
euxis-loop stop background-tasks
```

**See Also:** [euxis-playbook](#euxis-playbook), [euxis-daemon](#euxis-daemon)

---

### euxis-hooks

**Synopsis:** `euxis-hooks <command> [hook-name]`

**Description:**
Git hooks management for automated quality gates and workflow enforcement.

**Commands:**
- `install` - Install git hooks
- `uninstall` - Remove git hooks
- `list` - Show installed hooks

**Examples:**
```bash
euxis-hooks install pre-commit
euxis-hooks list
euxis-hooks uninstall pre-push
```

**See Also:** [euxis-git-guard](#euxis-git-guard)

---

### euxis-git-guard

**Synopsis:** `euxis-git-guard [command]`

**Description:**
Git operation protection and safety enforcement to prevent destructive actions.

**Examples:**
```bash
euxis-git-guard enable
euxis-git-guard check-force-push
```

**See Also:** [euxis-hooks](#euxis-hooks)

---

## User Interface

### euxis-ui

**Synopsis:** `euxis-ui <command> [args]`

**Description:**
User interface tools and components for Euxis interaction.

**Commands:**
- Various UI-related operations

**Examples:**
```bash
euxis-ui start
euxis-ui dashboard
```

**See Also:** [euxis-voice](#euxis-voice), [euxis-slash](#euxis-slash)

---

### euxis-voice

**Synopsis:** `euxis-voice <command> [args]`

**Description:**
Voice interface for hands-free Euxis interaction and voice-driven workflows.

**Commands:**
- Voice processing and recognition commands

**Examples:**
```bash
euxis-voice start-session
euxis-voice process-audio input.wav
```

**See Also:** [euxis-ui](#euxis-ui)

---

### euxis-slash

**Synopsis:** `euxis-slash <command> [args]`

**Description:**
Slash command processor for quick Euxis operations via chat interfaces.

**Examples:**
```bash
euxis-slash process "/deploy staging"
euxis-slash register new-command
```

**See Also:** [euxis-ui](#euxis-ui)

---

## Utilities

### euxis-deploy

**Synopsis:** `euxis-deploy [target] [environment]`

**Description:**
Deployment operations and environment management.

**Examples:**
```bash
euxis-deploy staging
euxis-deploy production --confirm
```

**See Also:** [euxis-certify](#euxis-certify)

---

### euxis-sync-docs

**Synopsis:** `euxis-sync-docs [source] [target]`

**Description:**
Documentation synchronization across repositories and environments.

**Docs Tooling Note:**
Gateway schemas can be validated with `gateway/protocol_test.py`.
Optional dependencies live in `requirements-dev.txt` (`jsonschema`, `referencing`).
Gateway health can be checked with `gateway/smoke_test.py`.

**Examples:**
```bash
euxis-sync-docs
euxis-sync-docs local remote
```

**See Also:** [euxis-codex](#euxis-codex)

---

### euxis-gym

**Synopsis:** `euxis-gym <command> [args]`

**Description:**
Training and simulation environment for agent development and testing.

**Commands:**
- Training and simulation operations

**Examples:**
```bash
euxis-gym simulate incident-response
euxis-gym train new-agent
```

**See Also:** [euxis-bench](#euxis-bench)

---

## Platform Information

Euxis runs on macOS, Linux, and WSL. Paths adjust to your platform automatically.

| Item | Location |
|------|----------|
| Installation | `~/.euxis/` |
| Executables | `~/cli/bin/` (symlinked from `~/.euxis/cli/bin/`) |
| Configuration | `~/.euxis/config/` |
| Cortex Database | `~/.euxis/memory/cortex/db/` |
| Agent Outputs | `~/.euxis/data/projects/` |
| Logs | `~/.euxis/data/logs/` |

**Environment Variables:**

| Variable | Default | Purpose |
|----------|---------|---------|
| `EUXIS_HOME` | `~/.euxis` | Installation directory |
| `EUXIS_PROVIDER` | `claude` | Default AI provider |
| `EUXIS_PROJECT` | current dir | Active project context |

---

*Euxis v0.0.8 · Build something that matters.*

---

Designed by Sebastien Rousseau — https://sebastienrousseau.com
Engineered with Euxis — Enterprise Unified Execution Intelligence System — https://euxis.co
