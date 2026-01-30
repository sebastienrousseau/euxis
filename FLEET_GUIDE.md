# Euxis Fleet Guide

Build software systems that work.

Version 6.0

---

## Agent Registry

### Command Layer

Orchestrate complex work. Design systems. Govern quality.

| Agent | What It Does |
|-------|-------------|
| `orchestrator` | Breaks down ambitious goals. Routes work to specialists. Delivers complete solutions. |
| `architect` | Designs systems that scale. Creates patterns that last. |
| `librarian` | Keeps knowledge sharp. Makes documentation beautiful. |

### Execution Layer

Ship code. Solve problems. Own your domain.

| Agent | Domain | Model Tier |
|-------|--------|------------|
| `automation-engineer` | Zero-touch deployments | Standard |
| `bug-fixer` | Get to root cause fast | Coding |
| `butler` | Perfect voice summaries | Utility |
| `compliance-officer` | Ship with confidence | Standard |
| `data-steward` | Total system visibility | Standard |
| `deep-researcher` | Find what others miss | Research |
| `devrel-advocate` | Build community | Standard |
| `edge-hunter` | Bulletproof security | Standard |
| `globalization-lead` | Global reach | Standard |
| `growth-marketer` | Scale your impact | Standard |
| `incident-commander` | Handle any crisis | Standard |
| `legacy-maintainer` | Modernize without breaking | Coding |
| `perf-optimizer` | Make it fast | Standard |
| `product-manager` | Ship what matters | Strategic |
| `qa-coordinator` | Ship with certainty | Standard |
| `release-manager` | Flawless releases | Standard |
| `reviewer` | Guarantee quality | Strategic |
| `social-manager` | Amplify your voice | Standard |
| `tech-writer` | Documentation that delights | Standard |
| `unit-tester` | Prevent regressions | Standard |
| `ux-sentinel` | Experiences users love | Standard |

---

## Dispatch Modes

Scale your team. Coordinate your way.

### Hierarchical

Central command for complex coordination.

```bash
euxis-dispatch manifest.json
```

Perfect for intricate projects requiring tight synchronization.

### Mesh

Specialists coordinate directly.

```bash
euxis-dispatch --mode mesh manifest.json
```

Dispatch-enabled: `architect`, `qa-coordinator`, `incident-commander`, `release-manager`

Best when domain experts know their workflows better than central command.

### Federated

Autonomous execution across boundaries.

```bash
euxis-dispatch --mode federated manifest.json
```

For cross-project work requiring independent operation.

### Dispatch Manifest

```json
{
  "project": "auth-service",
  "mode": "hierarchical",
  "dispatches": [
    {
      "agent": "edge-hunter",
      "priority": "P0",
      "task": "Scan for injection vulnerabilities",
      "verify_cmd": "security-scan --critical-only"
    }
  ]
}
```

---

## Memory Architecture

Remember everything. Learn from everything.

### Episodic
Capture what happened.
```bash
euxis-cortex remember "Fixed auth bug in login.py line 89" "bug-fixer" --type episodic
```

### Semantic
Store lasting knowledge.
```bash
euxis-cortex remember "API uses OAuth 2.0 with PKCE" "architect" --type semantic
```

### Procedural
Build repeatable workflows.
```bash
euxis-cortex remember "Deploy: test → build → tag → push → verify" "release-manager" --type procedural
```

### Memory Commands

```bash
# Store knowledge
euxis-cortex remember "<fact>" "<agent>" --type <episodic|semantic|procedural>

# Recall knowledge
euxis-cortex recall "<keywords>"
euxis-cortex recall "<keywords>" --type procedural
```

Always check for proven patterns:
```bash
euxis-cortex recall "deployment workflow" --type procedural
```

---

## Model Tiers

Right capability. Right cost.

| Tier | When to Use | Provider | Agents |
|------|-------------|----------|--------|
| Strategic | Complex reasoning, architecture | `claude` | orchestrator, architect, product-manager, reviewer |
| Research | Deep analysis, large context | `gemini` | deep-researcher |
| Coding | Code generation, surgical fixes | `opencode` | bug-fixer, legacy-maintainer |
| Utility | Fast operations, zero cost | `ollama` | butler, librarian |
| Standard | General purpose | `claude` | all others |

**P0 Override:** Critical work gets Strategic tier. Always.

**Explicit Override:** Specify any provider.
```bash
euxis bug-fixer "Fix auth.py" gemini
```

---

## Quality Gates

Ship with confidence.

### Layer 1: Internal Consistency
Every claim backed by evidence. No contradictions.

### Layer 2: Cross-Reference
Validate against stored knowledge. Surface conflicts.

### Layer 3: Review Checkpoint
The `reviewer` validates all multi-agent output.

### Learn from Failure
```
REFLEXION: Root cause analysis
EVIDENCE: Specific failure indicator  
STRATEGY: Different approach for retry
CONTRAINDICATION: Never repeat this mistake
```

Store as procedural memory. Get better every time.

---

## Conflict Resolution

When specialists disagree. Resolve systematically.

### Resolution Order
1. **Domain Priority** — Security beats performance. Correctness beats speed.
2. **Evidence Weight** — Verified data beats inference.
3. **Negotiation** — One round. Both agents state positions.
4. **Human Escalation** — Present evidence. Let you decide.

### Conflict Response Format
```json
{
  "agent": "edge-hunter",
  "position": "Reject this change",
  "evidence": "SQL injection in line 42",
  "confidence": "HIGH",
  "concession": "Fix injection, then approve"
}
```

One round. No endless debate.

---

## Delegation Patterns

Right specialist. Every time.

| Task | Lead | Support |
|------|------|---------|
| Security audit | `edge-hunter` | `compliance-officer` |
| Bug investigation | `bug-fixer` | `data-steward` |
| Architecture design | `architect` | `product-manager` |
| Release coordination | `release-manager` | `qa-coordinator`, `tech-writer` |
| Performance optimization | `perf-optimizer` | `architect` |
| Documentation | `tech-writer` | `librarian` |
| Research | `deep-researcher` | `architect` |
| Incident response | `incident-commander` | `bug-fixer`, `edge-hunter` |
| Test strategy | `qa-coordinator` | `unit-tester`, `edge-hunter` |

---

## Common Workflows

### Security Pipeline
```
edge-hunter → bug-fixer → unit-tester → qa-coordinator → reviewer
```

### Release Pipeline
```
qa-coordinator → release-manager → tech-writer → reviewer
```

### Incident Response
```
incident-commander → bug-fixer + edge-hunter → architect → qa-coordinator
```

### Research to Implementation
```
deep-researcher → architect → product-manager → bug-fixer → reviewer
```

---

## Usage

### Single Agent
```bash
euxis <agent> "<task>"
```

### Orchestrated Work
```bash
euxis orchestrator "<complex goal>"
```

### Parallel Dispatch
```bash
euxis-dispatch manifest.json
```

### Adversarial Review
```bash
euxis-council "<decision>"
```

### Autonomous Retry
```bash
euxis-loop <agent> "<task>" "<verify_cmd>"
```

---

*Euxis v6.0*

*Build something that matters.*
