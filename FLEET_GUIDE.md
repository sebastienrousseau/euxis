# Euxis Fleet Guide

Version: 5.0 | Last updated: 2026-01-30

Central registry of all 24 agents with delegation patterns, provider tiering, dispatch modes, and workflow examples.

---

## Agent Index

### Core Infrastructure (3)

These agents form the command layer. They decompose, coordinate, and govern — they do not implement.

| Agent | Role | Provider Tier |
|-------|------|---------------|
| `orchestrator` | Task decomposition, delegation, synthesis | Strategic (claude) |
| `architect` | Software architecture, patterns, ADRs, fleet dispatching | Strategic (claude) |
| `librarian` | Memory optimization, docs governance, compliance | Utility (ollama) |

### Fleet Specialists (21)

These agents execute specialist work. Each owns a well-defined domain.

| Agent | Role | Provider Tier |
|-------|------|---------------|
| `automation-engineer` | CI/CD pipelines, IaC, Docker, Terraform | Standard (claude) |
| `bug-fixer` | Debugging, root cause analysis, surgical fixes | Coding (opencode) |
| `butler` | TTS-optimized summarization, spoken English output | Utility (ollama) |
| `compliance-officer` | PII scanning, license auditing, GDPR/CCPA compliance | Standard (claude) |
| `data-steward` | Observability, telemetry, structured logging | Standard (claude) |
| `deep-researcher` | Iterative multi-pass research with cross-validation | Research (gemini) |
| `devrel-advocate` | Developer relations, tutorials, demos, conferences | Standard (claude) |
| `edge-hunter` | Security analysis, boundary testing, vulnerability assessment | Standard (claude) |
| `globalization-lead` | i18n, l10n, RTL support, Unicode validation | Standard (claude) |
| `growth-marketer` | SEO, AARRR funnel, CRO, GTM strategy | Standard (claude) |
| `incident-commander` | Incident response, root cause analysis, post-mortems | Standard (claude) |
| `legacy-maintainer` | Legacy code documentation, non-breaking upgrades | Coding (opencode) |
| `perf-optimizer` | Latency, throughput, memory profiling | Standard (claude) |
| `product-manager` | Requirements, user stories, MoSCoW prioritization | Strategic (claude) |
| `qa-coordinator` | E2E testing coordination, quality gate enforcement | Standard (claude) |
| `release-manager` | Changelogs, semantic versioning, release coordination | Standard (claude) |
| `reviewer` | Quality gate, output validation, completeness checking | Strategic (claude) |
| `social-manager` | Platform-native content, calendars, community engagement | Standard (claude) |
| `tech-writer` | Documentation, tutorials, API reference, glossary | Standard (claude) |
| `unit-tester` | Test coverage, reliability, regression prevention | Standard (claude) |
| `ux-sentinel` | Accessibility (WCAG 2.1 AA), design system, responsive testing | Standard (claude) |

---

## Hybrid Dispatch Architecture (v5.0)

The fleet supports three dispatch modes, selected per-phase by the orchestrator:

### Hierarchical (Default)

All agents report to the orchestrator. Use for standard coordination where centralized synthesis is needed.

```bash
euxis-dispatch manifest.json                    # default: hierarchical
euxis-dispatch --mode hierarchical manifest.json
```

### Mesh (Peer-to-Peer)

Agents with dispatch authority coordinate sub-workflows directly without routing through the orchestrator. Results still flow back to the orchestrator for final synthesis.

**Dispatch-authority agents:** `architect`, `qa-coordinator`, `incident-commander`, `release-manager`

```bash
euxis-dispatch --mode mesh manifest.json
```

Use when a specialist agent can coordinate a focused sub-workflow more efficiently than routing through the orchestrator (e.g., `qa-coordinator` dispatching `unit-tester` and `edge-hunter` for a test strategy).

### Federated

Agents operate autonomously across project boundaries and contribute results to a shared synthesis. Each agent gets `[FEDERATED MODE]` context with its project scope.

```bash
euxis-dispatch --mode federated manifest.json
```

Use for cross-project tasks where agents need to work independently on different codebases.

### Dispatch Manifest Format

```json
{
  "project": "my-app",
  "mode": "hierarchical",
  "dispatches": [
    {
      "agent": "edge-hunter",
      "priority": "P0",
      "task": "Scan auth module for injection vulnerabilities",
      "verify_cmd": "grep -c CRITICAL edge-hunter-output.md | grep -q 0"
    },
    {
      "agent": "unit-tester",
      "priority": "P1",
      "task": "Add boundary tests for the login endpoint",
      "verify_cmd": "pytest tests/test_login.py"
    }
  ]
}
```

**Rules:**
- Every dispatch MUST include `agent`, `priority`, `task`, and `verify_cmd`.
- `verify_cmd` MUST be a non-interactive command that exits 0 on success.
- Dispatches with no mutual dependencies are grouped for parallel execution.
- `mode` MUST be one of: `hierarchical`, `mesh`, `federated`.

---

## Tri-Typed Memory System (v5.0)

All Cortex memories are classified into three types:

| Type | Description | Store When | Example |
|------|-------------|------------|---------|
| `episodic` | Specific events and outcomes | Task completed, bug fixed, test passed | `"Fixed null pointer in auth.py line 89 by adding guard clause"` |
| `semantic` | General facts and relationships | Discovered codebase fact, API contract, config rule | `"The auth module uses JWT tokens with RS256 signing"` |
| `procedural` | Reusable workflows and contraindications | Learned a workflow, found a pattern, failed approach to avoid | `"CONTRAINDICATION: Do NOT retry with expired refresh tokens"` |

### Memory Commands

```bash
# Store with type
euxis-cortex remember "Bug #42 fixed by null-check in auth.py" "bug-fixer" --type episodic
euxis-cortex remember "API uses OAuth 2.0 with PKCE flow" "architect" --type semantic
euxis-cortex remember "To deploy: tests -> build -> tag -> push -> verify" "release-manager" --type procedural

# Recall with optional type filter
euxis-cortex recall "authentication"
euxis-cortex recall "deployment workflow" --type procedural
```

### Pre-Task Recall (Mandatory)

Before starting complex tasks, agents MUST query the Cortex for relevant PROCEDURAL memories:

```bash
euxis-cortex recall "<task keywords>" --type procedural
```

If a PROCEDURAL memory contains a CONTRAINDICATION tag, agents MUST avoid the flagged approach.

---

## Conflict Resolution Protocol (v5.0)

When multiple agents produce conflicting outputs during orchestrated work:

### Detection

The orchestrator (or `reviewer` during Layer 3 checkpoint) detects conflicts by comparing findings, recommendations, and artifacts across agent outputs.

### Resolution Hierarchy

Applied in order:

1. **Domain Priority** — The agent with primary scope for the conflict domain wins:
   - Security conflicts → `edge-hunter`
   - Architecture conflicts → `architect`
   - User-facing conflicts → `product-manager`
   - Performance vs correctness → correctness wins
2. **Evidence Weight** — Verified data > inference > heuristic.
3. **Negotiation Round** — Each conflicting agent produces a `CONFLICT_RESPONSE`:
   ```json
   {"agent": "<id>", "position": "<recommendation>", "evidence": "<data>", "confidence": "HIGH|MEDIUM|LOW", "concession": "<acceptable compromise>"}
   ```
   Maximum 1 round. Unbounded negotiation is forbidden.
4. **Human Escalation** — If negotiation fails, both positions are presented to the user with evidence summaries.

### Reviewer Conflict Report

The `reviewer` agent produces a Conflict Report table during Layer 3 validation:

| # | Agent A | Agent B | Conflict | Resolution |
|---|---------|---------|----------|------------|
| 1 | `<agent>`: position | `<agent>`: position | description | PRIORITY / EVIDENCE / NEGOTIATE / ESCALATE |

---

## 3-Layer Self-Correction (v5.0)

Every agent applies layered verification before producing FINAL ANSWER:

### Layer 1 — Internal Consistency (All Agents)

Before FINAL ANSWER, verify:
- Every claim is supported by a specific OBSERVATION from the ReAct loop
- No OBSERVATION was ignored or contradicted without justification
- Output format matches the agent's declared `## Output Format`
- No file paths are fabricated — every path was verified by an ACTION

If any check fails, add another THOUGHT/ACTION/OBSERVATION cycle.

### Layer 2 — Cross-Reference (Research/Analysis Tasks)

- Cross-reference key findings against Cortex memories
- Flag contradictions: `[CONTRADICTION: <new finding> vs <stored memory>]`
- Mark novel findings: `[NEW — NO PRIOR REFERENCE]`

### Layer 3 — Evaluator Checkpoint (Multi-Agent Work)

- The `reviewer` agent validates all synthesized outputs before user delivery
- Checks: completeness, consistency across agent outputs, conflict detection, adherence to success criteria
- If rejected, the orchestrator replans the failing phase

### Reflexion Protocol (On WARNING or FAILURE)

```
REFLEXION: <Root cause — not the symptom>
EVIDENCE: <Specific OBSERVATION that revealed the failure>
STRATEGY: <Concrete alternative approach — MUST differ from the failed one>
CONTRAINDICATION: <What approach to NEVER repeat for this class of problem>
```

Stored as PROCEDURAL memory:
```bash
euxis-cortex remember "CONTRAINDICATION: <approach to avoid>. Use <alternative> instead." "<agent_id>" --type procedural
```

---

## Delegation Patterns

### When to Delegate What

| Task Type | Primary Agent | Supporting Agents |
|-----------|---------------|-------------------|
| New feature design | `architect` | `product-manager`, `ux-sentinel` |
| Bug investigation | `bug-fixer` | `data-steward`, `unit-tester` |
| Security audit | `edge-hunter` | `compliance-officer`, `architect` |
| Release preparation | `release-manager` | `qa-coordinator`, `reviewer`, `tech-writer` |
| Performance issue | `perf-optimizer` | `data-steward`, `architect` |
| Documentation gap | `tech-writer` | `librarian`, `devrel-advocate` |
| Research question | `deep-researcher` | `architect`, `product-manager` |
| Incident response | `incident-commander` | `bug-fixer`, `edge-hunter`, `architect` |
| Test strategy | `qa-coordinator` | `unit-tester`, `edge-hunter`, `perf-optimizer` |
| Legacy migration | `legacy-maintainer` | `architect`, `unit-tester` |
| Internationalization | `globalization-lead` | `ux-sentinel`, `tech-writer` |
| Compliance check | `compliance-officer` | `edge-hunter`, `librarian` |
| Go-to-market | `growth-marketer` | `social-manager`, `devrel-advocate` |
| Memory cleanup | `librarian` | `data-steward` |
| Voice summary | `butler` | None |

### Multi-Agent Workflows

**Security Hardening Pipeline:**
```
edge-hunter → bug-fixer → unit-tester → qa-coordinator → reviewer
```

**Release Pipeline:**
```
qa-coordinator → release-manager → tech-writer → reviewer → butler (announce)
```

**Incident Response:**
```
incident-commander → bug-fixer + edge-hunter → architect → qa-coordinator → release-manager
```

**Research-to-Implementation:**
```
deep-researcher → architect → product-manager → bug-fixer → unit-tester → reviewer
```

**Documentation Refresh:**
```
librarian (audit) → tech-writer (write) → reviewer (validate) → librarian (sync)
```

---

## Invoking Agents

### Single Agent
```bash
euxis <agent> "<task>" [provider]
```

### Orchestrated Multi-Agent
```bash
euxis orchestrator "<high-level goal>"
```

### Parallel Dispatch (3 Modes)
```bash
euxis architect "<audit task>" > manifest.json
euxis-dispatch manifest.json                      # hierarchical (default)
euxis-dispatch --mode mesh manifest.json           # peer-to-peer
euxis-dispatch --mode federated manifest.json      # cross-project
```

### Adversarial Debate
```bash
euxis-council "<architectural decision>"
```

### Autonomous Retry
```bash
euxis-loop <agent> "<task>" "<verify_cmd>" [retries]
```

---

## Provider Tiering (5-Tier Cost-Optimized)

| Tier | Agents | Provider | Reason |
|------|--------|----------|--------|
| Strategic | orchestrator, architect, product-manager, reviewer | `claude` | Best reasoning and tool use |
| Research | deep-researcher | `gemini` | 2M context window |
| Coding | bug-fixer, legacy-maintainer | `opencode` | Fast local code models |
| Utility | butler, librarian | `ollama` | Zero latency, no cost |
| Standard | all others | `claude` | General-purpose fallback |

**P0 Override:** Tasks marked P0 (CRITICAL) always route to the Strategic tier (`claude`) regardless of agent assignment.

An explicit provider argument always overrides tiering:
```bash
euxis bug-fixer "Fix user.py" gemini    # explicit override
```
