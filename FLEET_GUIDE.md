# Euxis Fleet Guide

**Enterprise Unified eXecution Intelligence System**

Version 0.0.7

---

## Agent Registry

See [CONSTITUTION.md](CONSTITUTION.md) for the authoritative governance document.

### Core (7) — Authority-bearing, always present

Seven core agents define direction and may block progress. If one is missing, the system is incomplete.

| Agent | What It Does | Model Tier |
|-------|-------------|------------|
| `orchestrator` | Breaks down ambitious goals. Routes work to specialists. Delivers complete solutions. | Strategic |
| `architect` | Designs systems that scale. Creates patterns that last. | Strategic |
| `product-manager` | Defines intent, scope, and prioritization. Ships what matters. | Strategic |
| `reviewer` | Truth & quality gate. Guarantees correctness and completeness. | Strategic |
| `librarian` | Keeps knowledge sharp. Documentation governance. | Utility |
| `compliance-officer` | Legal, privacy, and regulatory authority. Ships with confidence. | Strategic |
| `system-critic` | Challenges assumptions. Surfaces hidden risks. Pre-mortems. | Strategic |

### Default (17) — Auto-available, task-triggered

Execute within scope when triggered. Advise but do not define direction.

| Agent | Domain | Model Tier |
|-------|--------|------------|
| `automation-engineer` | Zero-touch deployments | Coding |
| `bug-fixer` | Get to root cause fast | Coding |
| `data-steward` | Total system visibility | Math/Logic |
| `edge-hunter` | Bulletproof security | Standard |
| `incident-commander` | Handle any crisis | Enterprise |
| `legacy-maintainer` | Modernize without breaking | Local Code |
| `perf-optimizer` | Make it fast | Math/Logic |
| `qa-coordinator` | Ship with certainty | Standard |
| `release-manager` | Flawless releases | Standard |
| `security-lead` | Security policy, threat governance, edge-hunter dispatch | Standard |
| `tech-writer` | Documentation that delights | Utility |
| `unit-tester` | Prevent regressions | Coding |
| `ux-sentinel` | Experiences users love | Standard |
| `cli-ui-artisan` | Terminal UI design and keyboard navigation | Standard |
| `web-ui-architect` | Web UI components and design systems | Standard |
| `theming-and-motion-engineer` | Theming, color systems, and animation | Standard |
| `interaction-and-input-specialist` | Keyboard navigation and input handling | Standard |

### On-Demand (7) — Explicit invocation only

Add leverage, not safety. Never block, never override core authority.

| Agent | Domain | Model Tier |
|-------|--------|------------|
| `brand-evangelist` | Brand voice and storytelling | Standard |
| `butler` | Perfect voice summaries | Utility |
| `deep-researcher` | Find what others miss | Research |
| `devrel-advocate` | Build community | Standard |
| `globalization-lead` | Global reach | Standard |
| `growth-marketer` | Scale your impact | Standard |
| `social-manager` | Amplify your voice | Standard |

### Specialist (4) — Domain-specific expertise

Deep domain knowledge for specialized auditing and compliance.

| Agent | Domain | Model Tier |
|-------|--------|------------|
| `crypto-cryptography-auditor` | Constant-time, key management, PQC readiness | Domain (claude) |
| `payments-domain-steward` | ISO 20022, schema validation, regulatory | Domain (claude) |
| `realtime-audio-engineer` | Latency budgets, buffer management, platform audio | Systems (goose) |
| `rust-crate-steward` | MSRV, feature flags, semver, docs.rs | Systems (goose) |

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

Every dispatch starts with a manifest. Here is the format:

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

Every agent has access to persistent memory. The Cortex stores three types of knowledge.

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
euxis-cortex remember "Deploy: test -> build -> tag -> push -> verify" "release-manager" --type procedural
```

### Memory Commands

```bash
# Store knowledge
euxis-cortex remember "<fact>" "<agent>" --type <episodic|semantic|procedural>

# Recall knowledge
euxis-cortex recall "<keywords>"
euxis-cortex recall "<keywords>" --type procedural
```

Always check for proven patterns before starting complex work:
```bash
euxis-cortex recall "deployment workflow" --type procedural
```

---

## Model Tiers

Right capability. Right cost.

| Tier | When to Use | Provider | Agents |
|------|-------------|----------|--------|
| S-Tier: Strategic | Complex reasoning, architecture | `claude` | orchestrator, architect, product-manager, reviewer, system-critic, compliance-officer |
| A-Tier: Research | Deep analysis, large context | `gemini` | deep-researcher |
| A-Tier: Enterprise | AWS infrastructure, corporate security | `amazon-q` | incident-commander |
| A-Tier: Domain | Domain-specific deep reasoning | `claude` | crypto-cryptography-auditor, payments-domain-steward |
| B-Tier: Coding | Agentic tool use, developer workflows | `goose` | bug-fixer, unit-tester, automation-engineer |
| B-Tier: Systems | Systems-level tool use | `goose` | realtime-audio-engineer, rust-crate-steward |
| B-Tier: Local Code | Local models for diffs, migrations | `opencode` | legacy-maintainer |
| B-Tier: Math/Logic | Algorithmic optimization, dense logic | `qwen` | perf-optimizer, data-steward |
| C-Tier: Utility | Summaries, formatting, zero cost | `ollama` | butler, librarian, tech-writer |
| Standard | General purpose | `claude` | all others |

**P0 Override:** Critical work gets S-Tier. Always.

**Explicit Override:** Specify any of the 10 providers.
```bash
euxis bug-fixer "Fix auth.py" gemini
euxis architect "Review this module" qwen
euxis deep-researcher "Audit all dependencies" amazon-q
```

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

Route work to the right specialist every time.

| Task | Lead | Support |
|------|------|---------|
| Security audit | `security-lead` | `edge-hunter`, `compliance-officer` |
| Bug investigation | `bug-fixer` | `data-steward` |
| Architecture design | `architect` | `product-manager` |
| Release coordination | `release-manager` | `qa-coordinator`, `tech-writer` |
| Performance optimization | `perf-optimizer` | `architect` |
| Documentation | `tech-writer` | `librarian` |
| Research | `deep-researcher` | `architect` |
| Incident response | `incident-commander` | `bug-fixer`, `edge-hunter` |
| Test strategy | `qa-coordinator` | `unit-tester`, `edge-hunter` |
| Cryptographic audit | `crypto-cryptography-auditor` | `edge-hunter`, `reviewer` |
| Payments integration | `payments-domain-steward` | `compliance-officer`, `unit-tester` |
| Audio pipeline | `realtime-audio-engineer` | `perf-optimizer`, `automation-engineer` |
| Rust crate release | `rust-crate-steward` | `unit-tester`, `edge-hunter` |

---

## Common Workflows

### Security Pipeline
```
security-lead -> edge-hunter -> bug-fixer -> unit-tester -> qa-coordinator -> reviewer
```

### Release Pipeline
```
qa-coordinator -> release-manager -> tech-writer -> reviewer
```

### Incident Response
```
incident-commander -> bug-fixer + edge-hunter -> architect -> qa-coordinator
```

### Research to Implementation
```
deep-researcher -> architect -> product-manager -> bug-fixer -> reviewer
```

---

## Squads

Six cross-functional teams with clear ownership.

| Squad | Purpose | Lead | Members |
|-------|---------|------|---------|
| Vision | Strategy | `orchestrator` | orchestrator, architect, product-manager, deep-researcher |
| Build | Execution | `bug-fixer` | bug-fixer, legacy-maintainer, automation-engineer, unit-tester |
| Quality | Assurance | `reviewer` | reviewer, qa-coordinator, edge-hunter, compliance-officer, perf-optimizer |
| Growth | Amplification | `tech-writer` | tech-writer, brand-evangelist, social-manager, devrel-advocate, growth-marketer, globalization-lead |
| Experience | UI Excellence | `web-ui-architect` | web-ui-architect, cli-ui-artisan, theming-and-motion-engineer, interaction-and-input-specialist, ux-sentinel |
| Specialist | Domain Expertise | `crypto-cryptography-auditor` | crypto-cryptography-auditor, payments-domain-steward, realtime-audio-engineer, rust-crate-steward |

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
| Steve Jobs | product-manager -> architect -> brand-evangelist -> reviewer | Vision to polished review |
| Fort Knox | edge-hunter -> compliance-officer -> qa-coordinator -> reviewer | Maximum security assurance |
| Content Factory | tech-writer -> brand-evangelist -> social-manager -> reviewer | End-to-end content production |
| Jony Ive | web-ui-architect -> theming-and-motion-engineer -> interaction-and-input-specialist -> ux-sentinel -> reviewer | Apple-level UI design and interaction |
| Crypto Fortress | security-lead -> crypto-cryptography-auditor -> edge-hunter -> reviewer | Deep cryptographic audit with security review |
| SWIFT Payment | payments-domain-steward -> compliance-officer -> unit-tester -> reviewer | End-to-end payments integration validation |

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
