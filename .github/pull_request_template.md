## Summary

<!-- Describe the change in 1-3 bullet points -->

## Test Plan

<!-- How was this tested? -->

- [ ] `euxis-health` — 8-point fleet integrity check passes
- [ ] `euxis-lint` — Static analysis clean
- [ ] `euxis-certify` — All 6 gates pass
- [ ] Manual verification

## Quality Checklist

- [ ] Code follows project conventions (`set -uo pipefail` in bash scripts)
- [ ] Documentation updated (README.md, docs/user-guide.md, docs/fleet-guide.md)
- [ ] No secrets or credentials committed
- [ ] Branding signature present in all commits
- [ ] JSON manifests valid (registry.json, squads.json, codex.json)
- [ ] Agent prompts include required header fields (agent_id, role, version, tags, last_updated)

## Certification Gates

| Gate | Check | Status |
|:-----|:------|:-------|
| 1+2 | Static Analysis (Lint) | |
| 3 | Infrastructure Tests | |
| 4 | Semantic Verification | |
| 5 | Branding Compliance | |
| 6 | Documentation Governance | |

---

> 🎨 Designed by **[Sebastien Rousseau](https://sebastienrousseau.com/)**
> 🚀 Engineered with **[Euxis](https://euxis.co/)** — Enterprise Unified eXecution Intelligence System
