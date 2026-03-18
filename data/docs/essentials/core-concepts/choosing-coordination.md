# Choosing Coordination Patterns

**When to use agents, combos, squads, or playbooks.**

Euxis offers four ways to coordinate work. Each pattern fits different situations.

---

## The Four Patterns

| Pattern | Execution | Best For |
|---------|-----------|----------|
| **Single Agent** | One agent, one task | Quick questions, focused work |
| **Combo** | Sequential chain | Iterative refinement, multiple perspectives |
| **Squad** | Parallel team | Full analysis, broad coverage |
| **Playbook** | Multi-phase workflow | Complex projects, repeatable processes |

---

## Single Agent

**Use when:** You have a focused question or task in one domain.

```bash
euxis debugger "Why does this function return null?"
```

### Characteristics
- Fast execution
- Deep expertise in one area
- No coordination overhead

### Example Scenarios
- Quick code review
- Single bug fix
- Answering a technical question
- Writing a single document

---

## Combo

**Use when:** You need multiple perspectives applied in sequence.

```bash
euxis-combo run envision "Design the onboarding flow"
```

### Characteristics
- Each agent builds on previous output
- Ordered execution (A → B → C → D)
- Good for iterative refinement

### How It Works

```
Task: "Design onboarding flow"
    ↓
planner: Defines scope and requirements
    ↓
architect: Designs the structure
    ↓
evangelist: Refines the narrative
    ↓
reviewer: Validates quality
    ↓
Final Output
```

### Available Combos

| Combo | Chain | Use Case |
|-------|-------|----------|
| **envision** | planner → architect → evangelist → reviewer | Product design |
| **protect** | pentester → auditor → inspector → reviewer | Security audit |
| **craft** | writer → evangelist → strategist → reviewer | Content creation |
| **refine** | designer → animator → interactor → reviewer | UI design |
| **seal** | sentinel → cryptographer → pentester → reviewer | Crypto audit |
| **settle** | ledger → auditor → tester → reviewer | Payments |

### Example Scenarios
- Feature design (envision)
- Security review (protect)
- Writing and refining content (craft)

---

## Squad

**Use when:** You need full coverage from multiple specialists at once.

```bash
euxis-squad deploy quality "Audit the authentication module"
```

### Characteristics
- Parallel execution
- Multiple perspectives simultaneously
- Broad coverage, not deep iteration

### How It Works

```
Task: "Audit authentication"
    ↓
    ├── reviewer: Code quality
    ├── inspector: E2E testing
    ├── pentester: Vulnerabilities
    ├── auditor: Compliance
    ├── optimizer: Performance
    ├── watchdog: Regressions
    ├── polyglot: Language patterns
    └── arbiter: Conflict resolution
    ↓
Combined Output
```

### Available Squads

| Squad | Lead | Members | Use Case |
|-------|------|---------|----------|
| **Vision** | orchestrator | 6 agents | Strategic planning |
| **Build** | debugger | 6 agents | Implementation |
| **Quality** | reviewer | 8 agents | Verification |
| **Growth** | writer | 6 agents | Marketing/docs |
| **Experience** | designer | 4 agents | UI/UX |
| **Specialist** | cryptographer | 4 agents | Domain expertise |

### Example Scenarios
- Pre-release quality audit
- Comprehensive security review
- Full feature implementation
- Marketing launch

---

## Playbook

**Use when:** You have a multi-phase project with distinct stages.

```bash
euxis-playbook run zero-to-one "Launch the notification feature"
```

### Characteristics
- Sequential phases
- Each phase can deploy squads
- Checkpoints gate progression
- Repeatable process

### How It Works

```
Playbook: zero-to-one
    ↓
Phase 1: Vision Squad → Define and plan
    ↓ checkpoint
Phase 2: Build Squad → Implement
    ↓ checkpoint
Phase 3: Quality Squad → Verify
    ↓ checkpoint
Phase 4: Growth Squad → Announce
    ↓
Complete
```

### Available Playbooks

| Playbook | Phases | Use Case |
|----------|--------|----------|
| **zero-to-one** | Vision → Build → Quality → Growth | New feature launch |
| **legacy-overhaul** | Build → Quality → Vision | Modernization |
| **red-alert** | Quality → Build → Vision | Incident response |
| **verify-everything** | 6 sequential gates | Pre-release |

### Example Scenarios
- Launching a new product
- Major version release
- Incident response
- Legacy system migration

---

## Decision Tree

```
Is this a quick, focused task?
├── Yes → Single Agent
└── No
    ↓
    Do you need multiple perspectives?
    ├── Yes
    │   ↓
    │   Should they build on each other?
    │   ├── Yes → Combo (sequential)
    │   └── No → Squad (parallel)
    └── No
        ↓
        Is this a multi-phase project?
        ├── Yes → Playbook
        └── No → Single Agent (reconsider scope)
```

---

## Combining Patterns

Patterns can nest. A playbook phase can deploy a squad. A squad task can use a combo.

```bash
# Playbook that runs squads
euxis-playbook run zero-to-one "Launch feature"

# Squad for one phase
euxis-squad deploy quality "Phase 3: Verification"

# Combo for focused analysis
euxis-combo run protect "Security review before release"
```

---

## Quick Reference

| Situation | Pattern | Command |
|-----------|---------|---------|
| Quick question | Agent | `euxis debugger "..."` |
| Design refinement | Combo | `euxis-combo run envision "..."` |
| Security audit | Squad | `euxis-squad deploy quality "..."` |
| Product launch | Playbook | `euxis-playbook run zero-to-one "..."` |
| Not sure | Orchestrator | `euxis orchestrator "..."` |

---

*Euxis v0.0.10 · Build something that matters.*
