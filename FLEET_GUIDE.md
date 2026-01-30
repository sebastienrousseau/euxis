# Euxis Fleet Guide

Central registry of all 24 agents with delegation patterns, provider tiering, and workflow examples.

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
| `automation-engineer` | CI/CD pipelines, IaC, Docker, Terraform | Default (claude) |
| `bug-fixer` | Debugging, root cause analysis, surgical fixes | Coding (opencode) |
| `butler` | TTS-optimized summarization, spoken English output | Utility (ollama) |
| `compliance-officer` | PII scanning, license auditing, GDPR/CCPA compliance | Default (claude) |
| `data-steward` | Observability, telemetry, structured logging | Default (claude) |
| `deep-researcher` | Iterative multi-pass research with cross-validation | Research (gemini) |
| `devrel-advocate` | Developer relations, tutorials, demos, conferences | Default (claude) |
| `edge-hunter` | Security analysis, boundary testing, vulnerability assessment | Default (claude) |
| `globalization-lead` | i18n, l10n, RTL support, Unicode validation | Default (claude) |
| `growth-marketer` | SEO, AARRR funnel, CRO, GTM strategy | Default (claude) |
| `incident-commander` | Incident response, root cause analysis, post-mortems | Strategic (claude) |
| `legacy-maintainer` | Legacy code documentation, non-breaking upgrades | Coding (opencode) |
| `perf-optimizer` | Latency, throughput, memory profiling | Default (claude) |
| `product-manager` | Requirements, user stories, MoSCoW prioritization | Strategic (claude) |
| `qa-coordinator` | E2E testing coordination, quality gate enforcement | Strategic (claude) |
| `release-manager` | Changelogs, semantic versioning, release coordination | Default (claude) |
| `reviewer` | Quality gate, output validation, completeness checking | Strategic (claude) |
| `social-manager` | Platform-native content, calendars, community engagement | Default (claude) |
| `tech-writer` | Documentation, tutorials, API reference, glossary | Default (claude) |
| `unit-tester` | Test coverage, reliability, regression prevention | Default (claude) |
| `ux-sentinel` | Accessibility (WCAG 2.1 AA), design system, responsive testing | Default (claude) |

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

### Parallel Dispatch
```bash
euxis architect "<audit task>" > manifest.json
euxis-dispatch manifest.json
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

## Provider Tiering

| Tier | Agents | Provider | Reason |
|------|--------|----------|--------|
| Strategic | orchestrator, architect, product-manager, reviewer, incident-commander, qa-coordinator | `claude` | Best reasoning and tool use |
| Research | deep-researcher | `gemini` | 2M context window |
| Utility | butler, librarian | `ollama` | Zero latency, no cost |
| Coding | bug-fixer, legacy-maintainer | `opencode` | Fast local code models |
| Default | all others | `claude` | General-purpose fallback |

An explicit provider argument always overrides tiering:
```bash
euxis bug-fixer "Fix user.py" gemini    # explicit override
```
