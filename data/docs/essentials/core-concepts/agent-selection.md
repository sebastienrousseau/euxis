# Agent Selection

**How to pick the right agent for your task.**

With 53 agents, choosing the right one might seem overwhelming. This guide makes it simple.

---

## The Simple Rule

**Match the task to the domain.**

Each agent owns one domain. Use the agent whose domain matches your task.

| If you're doing... | Use... |
|-------------------|--------|
| Debugging code | `debugger` |
| Reviewing architecture | `architect` |
| Writing tests | `tester` |
| Finding vulnerabilities | `pentester` |
| Writing documentation | `writer` |
| Planning work | `planner` |

---

## Quick Lookup by Task

### Code & Debugging

| Task | Agent |
|------|-------|
| Fix a bug | `debugger` |
| Trace a crash | `investigator` |
| Optimize performance | `optimizer` |
| Upgrade dependencies | `maintainer` |
| Apply automated fixes | `repairer` |

### Architecture & Design

| Task | Agent |
|------|-------|
| Design a system | `architect` |
| Review structure | `architect` |
| Plan implementation | `planner` |
| Design UI components | `designer` |
| Design terminal UI | `tactician` |

### Testing & Quality

| Task | Agent |
|------|-------|
| Write unit tests | `tester` |
| Run E2E tests | `inspector` |
| Pre-merge analysis | `watchdog` |
| Review code quality | `reviewer` |
| Check language patterns | `polyglot` |

### Security

| Task | Agent |
|------|-------|
| Find vulnerabilities | `pentester` |
| Security policy review | `sentinel` |
| Compliance check | `auditor` |
| Crypto implementation | `cryptographer` |

### DevOps & Infrastructure

| Task | Agent |
|------|-------|
| CI/CD pipelines | `automaton` |
| Observability/logging | `telemetrist` |
| Incident response | `responder` |
| Release coordination | `gatekeeper` |

### Documentation & Communication

| Task | Agent |
|------|-------|
| Write docs | `writer` |
| Brand voice | `evangelist` |
| Developer tutorials | `ambassador` |
| Content strategy | `strategist` |
| Voice summaries | `butler` |

### Research

| Task | Agent |
|------|-------|
| Compare options | `researcher` |
| Deep analysis | `researcher` |
| Historical context | `historian` |

---

## When You're Not Sure

### Option 1: Ask the Orchestrator

The orchestrator will figure out which agents to use.

```bash
euxis orchestrator "I need to make our login more secure and add rate limiting"
```

It will analyze your goal and delegate to the right specialists.

### Option 2: Use Domain Keywords

Think about the domain of your task:

- **"bug," "fix," "error," "crash"** → `debugger`
- **"design," "architecture," "structure"** → `architect`
- **"test," "coverage," "assertion"** → `tester`
- **"security," "vulnerability," "injection"** → `pentester`
- **"document," "explain," "tutorial"** → `writer`
- **"compare," "research," "analyze"** → `researcher`
- **"deploy," "pipeline," "CI/CD"** → `automaton`
- **"performance," "latency," "optimize"** → `optimizer`

### Option 3: Start Broad, Go Specific

If unsure, start with a core agent:

1. `orchestrator`: General coordination
2. `architect`: System-level thinking
3. `debugger`: Code-level work
4. `researcher`: Information gathering

Then get more specific based on results.

---

## Agent Tiers

### Core Agents (Use These First)

These agents have authority. They can block work or define direction.

| Agent | When to Use |
|-------|-------------|
| `orchestrator` | Complex goals needing coordination |
| `architect` | System design and structure |
| `reviewer` | Quality validation |
| `planner` | Scoping and prioritization |
| `critic` | Risk assessment |

### Default Agents (Domain Specialists)

Use these for specific domain work.

| Agent | Domain |
|-------|--------|
| `debugger` | Bug fixing |
| `tester` | Test writing |
| `pentester` | Security testing |
| `writer` | Documentation |
| `automaton` | CI/CD |
| `optimizer` | Performance |

### On-Demand Agents (Growth & Communication)

Explicitly invoke for marketing and growth tasks.

| Agent | Domain |
|-------|--------|
| `researcher` | Deep research |
| `evangelist` | Brand voice |
| `marketer` | Growth strategy |
| `ambassador` | Developer relations |

### Specialist Agents (Deep Expertise)

Use only for specific domain needs.

| Agent | Domain |
|-------|--------|
| `cryptographer` | Cryptographic systems |
| `ledger` | Financial/payments |
| `conduit` | Real-time audio |
| `custodian` | Rust ecosystem |

---

## Common Mistakes

### Using `orchestrator` for Everything

The orchestrator coordinates work. For focused tasks, use domain agents directly.

❌ `euxis orchestrator "Fix the null pointer in auth.py"`
✅ `euxis debugger "Fix the null pointer in auth.py"`

### Using `architect` for Bug Fixes

The architect designs systems. For fixing bugs, use the debugger.

❌ `euxis architect "Fix the login bug"`
✅ `euxis debugger "Fix the login bug"`

### Skipping the Reviewer

For important work, always get a review.

```bash
euxis debugger "Fix the payment processing bug"
euxis reviewer "Review the payment processing fix"
```

---

## Decision Flowchart

```
What are you trying to do?
│
├── Fix something broken → debugger
├── Build something new → architect → debugger
├── Test something → tester or inspector
├── Secure something → pentester or sentinel
├── Document something → writer
├── Research something → researcher
├── Coordinate multiple things → orchestrator
└── Not sure → orchestrator
```

---

*Euxis v0.0.4 · Build something that matters.*
