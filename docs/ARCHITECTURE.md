# Euxis Architecture

## Overview

Euxis is a multi-provider AI agent orchestration framework with a two-tier agent hierarchy, tiered memory system, and capability-based task routing.

**Version:** 0.0.7
**Framework Size:** ~1,270 LOC (8 library modules + main entry point)
**Agent Count:** 38 (7 core + 31 fleet)

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         User Interface                          │
│              (CLI: euxis <agent> <task> [provider])            │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Dispatch Layer                             │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐ │
│   │  cli.sh      │  │ dispatch.sh  │  │ skill-detector.sh    │ │
│   │  (parsing)   │  │ (routing)    │  │ (auto-detection)     │ │
│   └──────────────┘  └──────────────┘  └──────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Agent Layer                                │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │                    CORE TIER (7)                        │  │
│   │  orchestrator │ architect │ product-manager │ reviewer  │  │
│   │  librarian │ compliance-officer │ system-critic         │  │
│   └─────────────────────────────────────────────────────────┘  │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │                   FLEET TIER (31)                       │  │
│   │  Default (20): bug-fixer, security-lead, tech-writer    │  │
│   │  On-Demand (7): deep-researcher, growth-marketer        │  │
│   │  Specialist (4): crypto-auditor, payments-steward       │  │
│   └─────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Memory Layer                                │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐ │
│   │  Hot Memory  │  │ Cold Memory  │  │ Cross-Agent Memory   │ │
│   │  (recent)    │  │ (archived)   │  │ (shared context)     │ │
│   └──────────────┘  └──────────────┘  └──────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Provider Layer                               │
│   claude │ gemini │ openai │ ollama │ opencode │ qwen │ ...    │
└─────────────────────────────────────────────────────────────────┘
```

## Agent Hierarchy

### Core Tier (Strategic)

Core agents handle high-level planning, governance, and quality assurance.

| Agent | Role | Capability Tags |
|-------|------|-----------------|
| **orchestrator** | Task decomposition, delegation, synthesis | task-decomposition, delegation, replanning |
| **architect** | System design, architecture review | architecture-review, code-review, design-patterns |
| **product-manager** | Requirements, planning | documentation, architecture-review |
| **reviewer** | Quality gates, validation | code-review, compliance-check, test-analysis |
| **librarian** | Memory optimization, knowledge management | documentation, memory-optimization |
| **compliance-officer** | Security, legal compliance | compliance-check, security-audit |
| **system-critic** | Risk assessment, challenge assumptions | architecture-review, security-audit |

### Fleet Tier (Tactical)

Fleet agents are specialists for specific domains and tasks.

**Default (20 — Auto-available, task-triggered):**
- automation-engineer, bug-fixer, crash-investigator, data-steward
- edge-hunter, incident-commander, language-specialist, legacy-maintainer
- perf-optimizer, qa-coordinator, regression-analyst, release-manager
- security-lead, tech-writer, unit-tester, ux-sentinel
- cli-ui-artisan, web-ui-architect, theming-and-motion-engineer
- interaction-and-input-specialist

**On-Demand (7 — Explicit invocation only):**
- brand-evangelist, butler, deep-researcher, devrel-advocate
- globalization-lead, growth-marketer, social-manager

**Specialist (4 — Domain-specific expertise):**
- crypto-cryptography-auditor, payments-domain-steward
- realtime-audio-engineer, rust-crate-steward

## Capability System

Capabilities define what agents can do and enable intelligent task routing.

### Capability Categories

| Category | Capabilities |
|----------|--------------|
| **Analysis** | code-review, architecture-review, test-analysis, performance-profiling |
| **Security** | security-audit, compliance-check |
| **Development** | debugging, documentation, design-patterns |
| **Operations** | cicd-pipeline, infrastructure-provisioning, build-optimization |
| **UI/UX** | tui-design, design-system, keyboard-navigation, theming |

### Capability Compatibility Matrix

```
code-review ←→ security-audit ←→ compliance-check
     ↕              ↕                   ↕
debugging ←→ performance-profiling ←→ test-analysis
```

## Data Flow

### Session Lifecycle

```
1. Command received: euxis <agent> <task>
2. Health check (fast boot validation)
3. Context detection (project type, domain)
4. Agent resolution (prompt lookup)
5. Memory retrieval (tiered: hot → relevant → cross-agent)
6. Prompt assembly (template substitution + protocols)
7. Provider execution (claude/gemini/openai/etc.)
8. Output capture (session storage)
9. Memory update (hot memory tier)
```

### Directory Structure

```
~/.euxis/
├── bin/                    # Executables
│   ├── euxis              # Main entry point
│   ├── euxis-*            # CLI tools (dispatch, loop, council, etc.)
│   └── lib/               # Library modules
├── config/                 # Configuration
│   ├── patterns/          # Validation patterns
│   └── templates/         # Prompt templates
├── data/                   # Runtime data
│   └── projects/          # Project-specific sessions
├── docs/                   # Documentation
│   └── adr/               # Architecture Decision Records
└── prompts/               # Agent prompts
    ├── core/              # Core tier agents
    ├── fleet/             # Fleet tier agents
    └── protocols/         # Shared protocol fragments
```

## Interaction Patterns

### Direct Invocation
```bash
euxis architect "review system design"
euxis security-lead "audit authentication flow"
```

### Delegation Chain
```
orchestrator → decomposes task → delegates to fleet agents → synthesizes results
```

### Council Mode
```bash
euxis-council "evaluate migration strategy"
# Multiple agents collaborate on complex decisions
```

### Loop Mode
```bash
euxis-loop architect "iteratively refine architecture"
# Agent works in cycles with checkpoints
```

## Protocol System

Protocols are reusable prompt fragments injected based on context:

- **constitution.md** - Core principles and constraints
- **Domain protocols** - Language-specific patterns (rust, python, etc.)
- **Task protocols** - Operation-specific guidance (review, debug, etc.)

## Memory Architecture

### Tiered Memory

| Tier | Scope | Retention | Purpose |
|------|-------|-----------|---------|
| **Hot** | Current session | Session | Immediate context |
| **Warm** | Recent sessions | 7 days | Relevant history |
| **Cold** | All sessions | Permanent | Long-term knowledge |

### Cross-Agent Memory

Enables agents to share context and avoid redundant work:
- Semantic drift detection prevents contradictions
- Auto-evolution updates shared knowledge graph

## Extension Points

### Adding New Agents

1. Create prompt file: `prompts/fleet/<agent-name>.txt`
2. Register in `registry.json`
3. Define capability tags

### Adding New Providers

1. Implement provider function in `lib/providers.sh`
2. Add to provider resolution logic

### Adding New Capabilities

1. Define in `capabilities.json`
2. Map to relevant agents in `registry.json`
