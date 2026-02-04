# Euxis Fleet Guide

**Enterprise Unified eXecution Intelligence System**

Version 0.0.7

---

## Fleet Overview

41 specialist agents organized into four tiers with distinct authority and operational rules. Each agent has a defined scope, optimal provider routing, and clear escalation paths.

### Core (9) — Authority-bearing, always present

Nine core agents define direction and may block progress. If one is missing, the system is incomplete.

| Agent | What It Does | Model Tier |
|-------|-------------|------------|
| `orchestrator` | Breaks down ambitious goals. Routes work to specialists. Delivers complete solutions. | Strategic |
| `architect` | Designs systems that scale. Creates patterns that last. | Strategic |
| `planner` | Defines intent, scope, and prioritization. Ships what matters. | Strategic |
| `reviewer` | Truth & quality gate. Guarantees correctness and completeness. | Strategic |
| `librarian` | Keeps knowledge sharp. Documentation governance. | Utility |
| `auditor` | Legal, privacy, and regulatory authority. Ships with confidence. | Strategic |
| `critic` | Challenges assumptions. Surfaces hidden risks. Pre-mortems. | Strategic |
| `arbiter` | Conflict resolution. Final decisions when agents disagree. | Strategic |
| `historian` | Long-term memory. Temporal patterns. Institutional knowledge. | Utility |

### Default (21) — Auto-available, task-triggered

Execute within scope when triggered. Advise but do not define direction.

| Agent | Domain | Model Tier |
|-------|--------|------------|
| `accountant` | Cost analysis and resource efficiency | Standard |
| `animator` | Theming, color systems, and animation | Standard |
| `automaton` | Zero-touch deployments | Coding |
| `debugger` | Get to root cause fast | Coding |
| `designer` | Web UI components and design systems | Standard |
| `gatekeeper` | Flawless releases | Standard |
| `inspector` | Ship with certainty | Standard |
| `interactor` | Keyboard navigation and input handling | Standard |
| `investigator` | Post-incident crash forensics | Standard |
| `maintainer` | Modernize without breaking | Local Code |
| `optimizer` | Make it fast | Math/Logic |
| `pentester` | Bulletproof security | Standard |
| `polyglot` | Language-specific pattern detection | Standard |
| `repairer` | Automated fixes and self-healing | Standard |
| `responder` | Handle any crisis | Enterprise |
| `sentinel` | Security policy and threat governance | Standard |
| `tactician` | Terminal UI design and keyboard navigation | Standard |
| `telemetrist` | Total system visibility | Math/Logic |
| `tester` | Prevent regressions | Coding |
| `watchdog` | Exhaustive pre-merge regression analysis | Standard |
| `writer` | Documentation that delights | Utility |

### On-Demand (7) — Explicit invocation only

Add leverage, not safety. Never block, never override core authority.

| Agent | Domain | Model Tier |
|-------|--------|------------|
| `ambassador` | Build community | Standard |
| `butler` | Perfect voice summaries | Utility |
| `evangelist` | Brand voice and storytelling | Standard |
| `localizer` | Global reach | Standard |
| `marketer` | Scale your impact | Standard |
| `researcher` | Find what others miss | Research |
| `strategist` | Amplify your voice | Standard |

### Specialist (4) — Domain-specific expertise

Activated for domain-scoped tasks only. Deep expertise in vertical domains.

| Agent | Domain | Model Tier |
|-------|--------|------------|
| `cryptographer` | Cryptographic correctness, PQC, hashing | Standard |
| `ledger` | ISO 20022, payments compliance, financial schemas | Standard |
| `conduit` | Real-time audio, latency, DSP, voice pipelines | Standard |
| `custodian` | Rust crate publishing, MSRV, docs | Standard |

**For the authoritative agent registry, governance rules, and authority model, see [CONSTITUTION.md](../CONSTITUTION.md).**

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

Dispatch-enabled: `architect`, `inspector`, `responder`, `gatekeeper`

Best when domain experts know their workflows better than central command.

### Federated

Autonomous execution across boundaries.

```bash
euxis-dispatch --mode federated manifest.json
```

For cross-project work requiring independent operation.

### Dispatch Manifest

Every dispatch starts with a manifest. Here is the format:

```json
{
  "project": "auth-service",
  "mode": "hierarchical",
  "dispatches": [
    {
      "agent": "pentester",
      "priority": "P0",
      "task": "Scan for injection vulnerabilities",
      "verify_cmd": "security-scan --critical-only"
    }
  ]
}
```

---

## Memory-Driven Operation

Agents leverage the Cortex for persistent, tri-typed memory across sessions. Before starting complex tasks, agents recall proven patterns and contraindications to avoid repeating failed approaches.

**For complete memory system documentation, commands, and type classification, see [User Guide](user-guide.md#tri-typed-memory-system).**

---

## Intelligence Routing

Euxis automatically routes agents to optimal AI providers based on task complexity and domain requirements. Critical (P0) tasks always use strategic-tier providers for maximum reliability.

**For the complete provider matrix, intelligence tiering, and explicit override syntax, see [User Guide](user-guide.md#ai-provider-matrix).**

---

## Quality Gates

Every agent applies three verification layers before delivering output.

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

When specialists disagree, Euxis resolves it systematically.

### Resolution Order
1. **Domain Priority** — Security beats performance. Correctness beats speed.
2. **Evidence Weight** — Verified data beats inference.
3. **Negotiation** — One round. Both agents state positions.
4. **Human Escalation** — Present evidence. Let you decide.

### Conflict Response Format
```json
{
  "agent": "pentester",
  "position": "Reject this change",
  "evidence": "SQL injection in line 42",
  "confidence": "HIGH",
  "concession": "Fix injection, then approve"
}
```

One round. No endless debate.

---

## Delegation Patterns

Route work to the right specialist every time.

| Task | Lead | Support |
|------|------|---------|
| Security audit | `sentinel` | `pentester`, `auditor` |
| Bug investigation | `debugger` | `telemetrist` |
| Architecture design | `architect` | `planner` |
| Release coordination | `gatekeeper` | `inspector`, `writer` |
| Performance optimization | `optimizer` | `architect` |
| Documentation | `writer` | `librarian` |
| Research | `researcher` | `architect` |
| Incident response | `responder` | `debugger`, `pentester` |
| Test strategy | `inspector` | `tester`, `pentester` |
| Cryptographic audit | `cryptographer` | `pentester`, `reviewer` |
| Payments integration | `ledger` | `auditor`, `tester` |
| Audio pipeline | `conduit` | `optimizer`, `automaton` |
| Rust crate release | `custodian` | `tester`, `pentester` |

---

## Common Workflows

### Security Pipeline
```
sentinel -> pentester -> debugger -> tester -> inspector -> reviewer
```

### Release Pipeline
```
inspector -> gatekeeper -> writer -> reviewer
```

### Incident Response
```
responder -> debugger + pentester -> architect -> inspector
```

### Research to Implementation
```
researcher -> architect -> planner -> debugger -> reviewer
```

---

## Squads

Six cross-functional teams with clear ownership.

| Squad | Purpose | Lead | Members |
|-------|---------|------|---------|
| Vision | Strategy | `orchestrator` | orchestrator, architect, planner, researcher, historian, accountant |
| Build | Execution | `debugger` | debugger, maintainer, automaton, tester, investigator, repairer |
| Quality | Assurance | `reviewer` | reviewer, inspector, pentester, auditor, optimizer, watchdog, polyglot, arbiter |
| Growth | Amplification | `writer` | writer, evangelist, strategist, ambassador, marketer, localizer |
| Experience | UI Excellence | `designer` | designer, tactician, animator, interactor |
| Specialist | Domain Expertise | `cryptographer` | cryptographer, ledger, conduit, custodian |

Deploy an entire squad with one command. Leads get P0 priority. Members get P1.

```bash
euxis-squad list                                # All squads
euxis-squad info build                          # Details
euxis-squad deploy quality "Audit auth module"  # Deploy squad
euxis-squad members vision                      # Members with roles
euxis-squad validate                            # Cross-check against registry
```

---

## Playbooks

Repeatable multi-phase workflows with squad-level automation.

| Playbook | Sequence | Use Case |
|----------|----------|----------|
| Zero to One | Vision -> Build -> Quality -> Growth | Full product launch |
| Legacy Overhaul | Build -> Quality -> Vision | Modernize legacy systems |
| Red Alert | Quality -> Build -> Vision | Emergency incident response |
| Verify Everything | 6-gate sequential pipeline | Comprehensive verification |
| Crypto Audit | Threat Model -> Crypto Audit -> Verification | Cryptographic security audit |
| Payments Integration | Schema Validation -> Integration Testing -> Release | ISO 20022 payments validation |
| Rust Release | Pre-Release Audit -> Quality & Security -> Release | Crate publishing pipeline |
| Audio Pipeline | Pipeline Audit -> Optimization -> Verification | Realtime audio optimization |

Each phase generates a dispatch manifest. Phases execute sequentially. Checkpoints gate progression. Abort-on-failure stops the pipeline when a critical phase fails.

```bash
euxis-playbook list                                             # Available playbooks
euxis-playbook info zero-to-one                                 # Phase breakdown
euxis-playbook run zero-to-one "Launch auth service" --dry-run  # Preview manifests
euxis-playbook run red-alert "Production DB outage"             # Execute emergency playbook
euxis-playbook status                                           # Session history
```

---

## Combos

Sequential agent chains. No manifest. No phases. Just fast, focused pipelines.

Each agent receives the original task plus the previous agent's output (capped at 4000 chars).

| Combo | Chain | Use Case |
|-------|-------|----------|
| Steve Jobs | planner -> architect -> evangelist -> reviewer | Vision to polished review |
| Fort Knox | pentester -> auditor -> inspector -> reviewer | Maximum security assurance |
| Content Factory | writer -> evangelist -> strategist -> reviewer | End-to-end content production |
| Jony Ive | designer -> animator -> interactor -> reviewer | Apple-level UI design and interaction |
| Crypto Fortress | sentinel -> cryptographer -> pentester -> reviewer | Deep cryptographic audit with security review |
| SWIFT Payment | ledger -> auditor -> tester -> reviewer | End-to-end payments integration validation |

```bash
euxis-combo list                                                # Available combos
euxis-combo info fort-knox                                      # Chain detail
euxis-combo run steve-jobs "Design onboarding flow"             # Execute chain
euxis-combo run fort-knox "Audit payment module" --provider claude  # Override provider
euxis-combo run jony-ive "Create a stunning dashboard UI"       # Apple-level design execution
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

### Codex Templates
```bash
euxis-codex list                                                    # Browse templates
euxis-codex run xray CODEX_CODEBASE_PATH=./src CODEX_ENTRY_POINT=main.py  # Execute
```

### Git Hooks
```bash
euxis-hooks install                     # Brand every commit automatically
euxis-hooks status                      # Check installation
```

---

*Euxis v0.0.7*

*Build something that matters.*
