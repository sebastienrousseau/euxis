# Euxis CLI Reference

Complete reference for the Euxis command-line interface.

## Quick Reference

```bash
euxis check .                               # Verify a repo (standard mode)
euxis triage .                              # Fast bounded triage (flash mode)
euxis review .                              # Deep verification (standard mode)
euxis review . --forensic                   # Forensic-depth verification
euxis certify-readiness . --framework soc2  # Certification readiness
euxis compare .                             # Compare triage vs deep verification
euxis stats --last 5                        # Recent validation metrics
euxis doctor                                # Environment diagnostics
euxis policy show                           # Inspect active policy
```

## Table of Contents

### Core Commands
- [euxis check](#euxis-check) - Verify a repository or target
- [euxis triage](#euxis-triage) - Fast bounded triage scan
- [euxis review](#euxis-review) - Deep verification (standard/forensic)
- [euxis certify-readiness](#euxis-certify-readiness) - Certification readiness assessment
- [euxis compare](#euxis-compare) - Compare triage vs deep verification
- [euxis stats](#euxis-stats) - Validation metrics and drift history
- [euxis policy](#euxis-policy) - Policy inspection and enforcement

### Lifecycle
- [euxis install](#euxis-install) - Bootstrap local installation
- [euxis update](#euxis-update) - Refresh metadata and registry
- [euxis upgrade](#euxis-upgrade) - Upgrade binary (pull + rebuild)
- [euxis uninstall](#euxis-uninstall) - Remove Euxis from this machine
- [euxis self](#euxis-self) - Installation introspection

### System
- [euxis doctor](#euxis-doctor) - Environment diagnostics
- [euxis health](#euxis-health) - Fleet integrity check
- [euxis verify](#euxis-verify) - Verify agent prompt integrity
- [euxis lint](#euxis-lint) - Lint agent prompts and configs

### Fleet (Advanced)
- [euxis playbook](#euxis-playbook) - Execute a playbook manifest
- [euxis agent](#euxis-agent) - Manage agents (list/register/unregister)
- [euxis combo](#euxis-combo) - Run agent combo (sequential pipeline)
- [euxis squad](#euxis-squad) - Squad orchestration
- [euxis dispatch](#euxis-dispatch) - Dispatch agents from manifest
- [euxis council](#euxis-council) - Multi-agent council deliberation
- [euxis loop](#euxis-loop) - Agent feedback loop
- [euxis synthesize](#euxis-synthesize) - Synthesize agent outputs
- [euxis ci](#euxis-ci) - CI-ready repo verdict

### Knowledge
- [euxis cortex](#euxis-cortex) - Semantic memory (remember/recall/forget)
- [euxis graph](#euxis-graph) - Knowledge graph operations
- [euxis codex](#euxis-codex) - Template codex (list/render/validate)

### Aliases

| Alias | Resolves to |
|-------|-------------|
| `quick` | `triage` |
| `deep` | `review` |
| `diag` | `doctor` |
| `metrics` | `stats` |
| `pb` | `playbook` |
| `verify-all` | `check` |

---

## Core Commands

### euxis check

**Synopsis:** `euxis check [target] [options]`

**Description:**
Primary repository verification command. Runs the verdict engine in standard mode by default.

**Arguments:**
- `target` - Path, URL, or named target. Defaults to current directory.

**Options:**
- `--triage` - Run fast triage instead of standard verification
- `--standard` - Force standard mode (default)
- `--forensic` - Run the most exhaustive verification
- `--policy [path]` - Apply policy evaluation
- `--ci` - Emit CI-safe output (JSON to stdout, TUI to stderr)
- `--json` - Emit artifact JSON to stdout

**Examples:**
```bash
euxis check .
euxis check /srv/project --ci
euxis check https://github.com/org/repo --policy
euxis check . --forensic --json
```

**Aliases:** `verify-all` resolves to `check`

**See Also:** [euxis triage](#euxis-triage), [euxis review](#euxis-review), [euxis playbook](#euxis-playbook)

---

### euxis triage

**Synopsis:** `euxis triage [target] [options]`

**Description:**
Fast bounded triage scan using flash mode. Runs 2 agents (librarian + reviewer) with a 75-second budget. Ideal for quick screening before deeper analysis.

**Arguments:**
- `target` - Path, URL, or named target. Defaults to current directory.

**Options:**
- `--policy [path]` - Apply policy evaluation
- `--ci` - Emit CI-safe output
- `--json` - Emit artifact JSON to stdout

**Examples:**
```bash
euxis triage .
euxis triage https://github.com/org/repo --ci
euxis triage /repo --json
```

**Aliases:** `quick` resolves to `triage`

**See Also:** [euxis check](#euxis-check), [euxis compare](#euxis-compare)

---

### euxis review

**Synopsis:** `euxis review [target] [options]`

**Description:**
Deep verification using standard mode (5 agents) or forensic mode (all 11 agents). Standard mode runs with a 10-minute budget; forensic with a 20-minute budget.

**Arguments:**
- `target` - Path, URL, or named target. Defaults to current directory.

**Options:**
- `--forensic` - Run forensic verification (all agents, 20-minute budget)
- `--policy [path]` - Apply policy evaluation
- `--ci` - Emit CI-safe output
- `--json` - Emit artifact JSON to stdout

**Examples:**
```bash
euxis review .
euxis review . --forensic
euxis review /repo --policy strict.json --ci
```

**Aliases:** `deep` resolves to `review`

**See Also:** [euxis check](#euxis-check), [euxis triage](#euxis-triage)

---

### euxis certify-readiness

**Synopsis:** `euxis certify-readiness [target] [options]`

**Description:**
Certification readiness assessment across 18 domains with framework-aware overlays. Runs 5 hard gates (commit signing, unit tests, build integrity, documentation, security) and evaluates domain coverage with evidence collection.

**Arguments:**
- `target` - Path to repository. Defaults to current directory.

**Subcommands:**
- `controls` - Print the 18-domain control model and critical gates
- `report <artifact>` - Pretty-print an existing certification artifact

**Options:**
- `--framework <general|soc2|iso27001>` - Framework overlay (default: general)
- `--strict` - Make soft gates blocking
- `--ci` - CI-safe output (TUI to stderr, JSON to stdout)
- `--json` - Emit artifact JSON to stdout
- `--no-build` - Skip build integrity gate
- `--no-tests` - Skip unit test health gate
- `--no-security` - Skip security critical gate
- `--commit-window <N>` - Number of recent commits to check for signing (default: 20)
- `--since-ref <ref>` - Check commits since this git ref
- `--output <path>` - Custom artifact output path

**Examples:**
```bash
euxis certify-readiness .
euxis certify-readiness . --framework soc2
euxis certify-readiness . --strict --json
euxis certify-readiness controls
euxis certify-readiness report ~/.euxis/data/runtime/sessions/latest_certification.json
```

**Exit Codes:**
- `0` - READY or READY WITH GAPS
- `1` - BLOCKED or INCONCLUSIVE
- `2` - Usage error or invalid framework

**Artifact:** Written to `$EUXIS_HOME/data/runtime/sessions/latest_certification.json`.

**See Also:** [euxis check](#euxis-check), [euxis audit](#euxis-audit-run)

---

### euxis compare

**Synopsis:** `euxis compare <target> [options]`

**Description:**
Runs both triage (flash) and standard verification on the same target, then produces an A/B comparison including verdict drift classification (SEMANTIC, MECHANICAL, or MIXED).

**Arguments:**
- `target` - **Required.** Path, URL, or named target.

**Options:**
- `--json` - Emit artifact JSON to stdout
- `--policy [path]` - Apply policy evaluation

**Examples:**
```bash
euxis compare .
euxis compare /path/to/repo
euxis compare https://github.com/org/repo
```

**See Also:** [euxis stats](#euxis-stats)

---

### euxis stats

**Synopsis:** `euxis stats [options]`

**Description:**
Displays validation metrics, drift history, and baseline compliance from the verdict history. Reads targets from `data/config/targets.json`.

**Options:**
- `--since <YYYY-MM-DD>` - Filter history by date
- `--last <N>` - Show only last N runs
- `--check-baseline` - Exit non-zero if targets are not met (CI mode)
- `--json` - Emit JSON output

**Examples:**
```bash
euxis stats
euxis stats --since 2026-03-14
euxis stats --last 10
euxis stats --check-baseline   # exit 1 if targets violated
```

**Aliases:** `metrics` resolves to `stats`

**See Also:** [euxis compare](#euxis-compare)

---

### euxis policy

**Synopsis:** `euxis policy <subcommand> [options]`

**Description:**
Policy inspection and enforcement. Manage and evaluate policy rules against verdict artifacts.

**Subcommands:**

- `show` - Display the active policy file
- `validate [path]` - Validate policy JSON syntax and check for unknown fields
- `check [target]` - Evaluate policy against the latest verdict artifact. If a target is provided, runs a fresh verification with policy applied.

**Examples:**
```bash
euxis policy show
euxis policy validate data/config/policy.json
euxis policy check               # check latest artifact
euxis policy check . --ci        # run fresh verification under policy
```

**See Also:** [euxis check](#euxis-check)

---

## Aliases

| Shorthand | Resolves to | Description |
|-----------|-------------|-------------|
| `quick` | `triage` | Fast screening alias |
| `deep` | `review` | Deep verification alias |
| `diag` | `doctor` | Diagnostics alias |
| `metrics` | `stats` | Metrics alias |
| `pb` | `playbook` | Playbook shorthand |
| `verify-all` | `check` | Compatibility alias |

---

## Lifecycle Commands

### euxis install

**Synopsis:** `euxis install`

**Description:** Bootstrap a local Euxis installation. Sets up data directories, agent registry, and configuration.

---

### euxis update

**Synopsis:** `euxis update`

**Description:** Refresh metadata and agent registry from upstream sources.

---

### euxis upgrade

**Synopsis:** `euxis upgrade`

**Description:** Pull latest changes and rebuild the binary (`git pull && make cpp-build`).

---

### euxis uninstall

**Synopsis:** `euxis uninstall`

**Description:** Remove Euxis from this machine. Cleans up symlinks, configuration, and data directories.

---

### euxis self

**Synopsis:** `euxis self`

**Description:** Installation introspection. Shows installation path, version, build info, and configuration status.

---

## System Commands

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

**Synopsis:** `euxis-daemon [interval_seconds]`

**Description:**
Runs `euxis-kaizen` on a timer with smart change detection. Default interval is 1800 seconds (30 minutes).

**Options:**
- `interval_seconds` - Override the default interval (seconds)
- `-h, --help` - Show help

**Examples:**
```bash
euxis-daemon
euxis-daemon 900
```

**See Also:** [euxis-kaizen](#euxis-kaizen)

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
- `--config <path>` - Path to gateway config (default: `~/.euxis/euxis-policy/gateway.json`)
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
### euxis health

**Synopsis:** `euxis health [--json]`

**Description:**
Fleet integrity check: agent registry, provider connectivity, build status, and test health.

**Options:**
- `--json` - JSON output
- `-h, --help` - Show help

**Examples:**
```bash
euxis health
euxis health --json
```

**See Also:** [euxis certify-readiness](#euxis-certify-readiness)

### euxis-verify

**Synopsis:** `euxis-verify [OPTIONS]`

**Description:**
Comprehensive release gate runner. Supports running all gates or specific subsets.

**Options:**
- `--all` - Run all checks (default for release)
- `--bench` - Performance benchmarks only
- `--branding` - Branding/persona consistency only
- `--audit` - Security audit only
- `--platform-gate` - Cross-platform compatibility only
- `--tests` - Unit test coverage gate only
- `--docs` - Documentation quality gate only
- `--legal` - Legal compliance gate only
- `--strict` - Exit 1 on any failure
- `--skip-agents` - Skip agent chain analysis
- `--json` - JSON output
- `-h, --help` - Show help

**Examples:**
```bash
euxis-verify --all
euxis-verify --docs --json
```

**See Also:** [euxis-verify-all](#euxis-verify-all)

### euxis-verify-all

**Synopsis:** `euxis-verify-all "<goal>" [--dry-run] [--from-gate N]`

**Description:**
Canonical sequential verification pipeline. Uses the playbook engine when available and falls back to direct delegation.

**Options:**
- `--dry-run` - Preview without executing
- `--from-gate N` - Start from gate N
- `-h, --help` - Show help

**Examples:**
```bash
euxis-verify-all "Release readiness for v0.0.10"
euxis-verify-all "Docs audit" --from-gate 2
```

**See Also:** [euxis-verify](#euxis-verify)

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
euxis-cortex remember 'Deployed v0.0.10 to staging, all tests passed' 'gatekeeper' --type episodic
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

**See Also:** [euxis certify-readiness](#euxis-certify-readiness), [euxis-audit-run](#euxis-audit-run)

---

### euxis certify-readiness

**Synopsis:** `euxis certify-readiness <target> [OPTIONS]`

**Description:**
18-domain certification readiness assessment with framework overlays and structured JSON artifacts.

**Options:**
- `--framework <general|soc2|iso27001>` - Framework overlay (default: general)
- `--strict` - Make soft gates blocking
- `--json` - JSON output to stdout
- `--ci` - CI-safe output mode

**Examples:**
```bash
euxis certify-readiness .
euxis certify-readiness . --framework soc2
euxis certify-readiness . --strict --json
```

**See Also:** [euxis check](#euxis-check), [euxis review](#euxis-review)

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

**See Also:** [euxis certify-readiness](#euxis-certify-readiness), [euxis-license-check](#euxis-license-check)

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

**See Also:** [euxis certify-readiness](#euxis-certify-readiness)

---

### euxis-sync-docs

**Synopsis:** `euxis-sync-docs [source] [target]`

**Description:**
Documentation synchronization across repositories and environments.

**Docs Tooling Note:**
Gateway schemas can be validated with `api/src/gateway/protocol_test.py`.
Optional dependencies live in `requirements-dev.txt` (`jsonschema`, `referencing`).
Gateway health can be checked with `api/src/gateway/smoke_test.py`.

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
| Build output | `~/.euxis/cmake-build/apps/cli/euxis-cli` |
| Configuration | `~/.euxis/data/config/` |
| Agent prompts | `~/.euxis/data/agents/prompts/` |
| Agent registry | `~/.euxis/data/agents/registry.db` |
| Verdicts | `~/.euxis/data/runtime/sessions/` |
| Cortex Database | `~/.euxis/data/runtime/memory/cortex/db/` |

**Environment Variables:**

| Variable | Default | Purpose |
|----------|---------|---------|
| `EUXIS_HOME` | `~/.euxis` | Installation directory |
| `EUXIS_DEFAULT_PROVIDER` | `claude` | Default AI provider |
| `EUXIS_LOCAL_ONLY` | unset | Force all routing to Ollama |
| `EUXIS_MODEL_OVERRIDE` | unset | Force a specific model |
| `EUXIS_DEFAULT_RESEARCH_PROVIDER` | unset | Override provider for research tasks |
| `EUXIS_DEFAULT_CODING_PROVIDER` | unset | Override provider for coding tasks |
| `EUXIS_DEFAULT_SECURITY_PROVIDER` | unset | Override provider for security tasks |
| `EUXIS_LOCAL_MODEL` | `qwen2.5-coder:7b` | Model for local-only mode |
| `EUXIS_TEST_MOCK_EXECUTION` | unset | Enable mock execution for tests |

---

*Euxis v0.0.10 · [euxis.co](https://euxis.co)*
