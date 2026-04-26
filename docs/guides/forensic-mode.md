# Forensic Mode Guide

Forensic mode is the highest-fidelity verification mode in euxis, deploying
all 11 agents with extended timeouts and top-tier model routing.

## When to Use

- Pre-release audits requiring maximum evidence density
- Security incident investigation
- Compliance-critical verification
- When standard mode produces INCONCLUSIVE verdicts

## Activation

```bash
# Via review command
euxis review --forensic .

# Via check command
euxis check --forensic .

# Via playbook directly
euxis playbook verify-everything --mode forensic .
```

## Agent Roster

All 11 agents from the `verify-everything` playbook are deployed:

| Index | Agent | Role | Model |
|-------|-------|------|-------|
| 0 | Librarian | Docs drift detection | Gemini 2.5 Pro |
| 1 | Writer | Documentation quality | Claude Sonnet |
| 2 | Strategist | Strategic risk assessment | Claude Opus |
| 3 | Architect | Structural risk detection | Claude Opus |
| 4 | Optimizer | Performance analysis | Claude Sonnet |
| 5 | Custodian | Observability & ops | Claude Sonnet |
| 6 | Automaton | CI/CD pipeline analysis | Gemini 2.5 Pro |
| 7 | Investigator | Deep research & forensics | Claude Opus |
| 8 | Sentinel | Security boundary enforcement | Claude Opus |
| 9 | Tester | Test quality assessment | Claude Sonnet |
| 10 | Reviewer | Decision synthesis | Claude Opus |

## SLA Budget

| Metric | Value |
|--------|-------|
| Target | 600s (10 min) |
| Soft cap | 900s (15 min) |
| Hard budget | 1200s (20 min) |
| Per-agent timeout | 300s (5 min) |

## Model Routing

Forensic mode uses `route_forensic()` with explicit per-agent overrides:

- **Opus** (highest reasoning): architect, strategist, investigator,
  sentinel, reviewer
- **Sonnet** (balanced): writer, optimizer, custodian, tester
- **Gemini Pro** (research): librarian, automaton

Opus is reserved exclusively for forensic mode — standard and flash modes
never use it.

## Interpreting Deep Verdicts

Forensic verdicts include:

- **Higher evidence density**: More execution-backed claims per agent
- **Cross-agent verification**: Findings corroborated across multiple agents
- **Contradiction detection**: Explicit flagging of inter-agent disagreements
- **Reflexion outcomes**: Agents that timed out may retry with refined prompts
- **Provenance markers**: SLSA v1 provenance tracking per invocation

## Cost Implications

Forensic mode is significantly more expensive than standard:

| Mode | Typical Cost | Duration |
|------|-------------|----------|
| Flash | ~$0.01 | ~45s |
| Standard | ~$0.10 | ~180s |
| Forensic | ~$2-5 | ~600s |

Cost depends on prompt length (repository size) and provider pricing.
Use `--local-only` to eliminate API costs (reduced quality).

## CI Usage

Forensic mode is not recommended for every CI run. Use it for:

```yaml
# Release pipeline only
release:
  script:
    - euxis review --forensic --ci --json .
  only:
    - tags
```
