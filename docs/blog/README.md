# `docs/blog/` — launch-week drafts (Executive Pragmatist format)

Source-of-truth Markdown for three launch-day blog posts. Each draft follows the full Executive Pragmatist publication template: complete YAML front matter (main + RSS + Apple + MS Application + Twitter Card + Humans.txt blocks), an Executive Summary leading on regulatory framing (DORA, BCBS 239, EU CRA, Basel III, PCI DSS, SR 11-7), numbered sections 01-10, two structured tables per article, a Mermaid diagram, a Boardroom Playbook + Bank-Type segmentation, an FAQ, and a numbered References section with proper citations.

Each draft is publish-ready. Republish by copying the file body to `sebastienrousseau.com/<slug>/` — the front matter maps onto the existing Static Site Generator (Kaishi) post schema. No translation needed.

## Drafts

| File | Audience | Slug |
|---|---|---|
| [`multi-llm-ensemble-sast.md`](multi-llm-ensemble-sast.md) | Security engineers + bank CIO/CISO offices + AppSec compliance leads | `2026-06-18-euxis-multi-llm-sast-ensemble-financial-infrastructure-2026` |
| [`cpp26-forward-compat-playbook.md`](cpp26-forward-compat-playbook.md) | C++ technical leads + investment-bank trading-system architects | `2026-06-18-euxis-cpp26-forward-compat-financial-trading-systems-2026` |
| [`signed-evidence-bundles.md`](signed-evidence-bundles.md) | DevSecOps + bank compliance / supply-chain risk leads | `2026-06-18-euxis-signed-evidence-pack-financial-infrastructure-2026` |

## Publication order on launch day

1. **`signed-evidence-bundles.md`** at T+0 — broadest compliance audience (DORA + EU CRA + BCBS 239 + PCI DSS), anchors the "why this exists" question.
2. **`multi-llm-ensemble-sast.md`** at T+4 hours — security/AppSec audience, references the CodeX-Verify paper and the SR 11-7 model-risk angle.
3. **`cpp26-forward-compat-playbook.md`** at T+1 day — C++ developer audience, completes the technical-credibility story.

Each post is in the 1,800-3,500 word range — full Executive Pragmatist depth, suitable for the bank-CIO Friday-afternoon reading slot.

## Voice and structural rules

- Lead with the regulatory anchor in the *first sentence after the H1*.
- Quick-answer block follows the lead paragraph — one-sentence definition for the AI-search and FAQ-schema audience.
- Executive summary is the second-screen anchor; readers skim it for the "what + why now + what changes" arc.
- Five Key Takeaways follow the Executive Pragmatist convention (bold lead phrase + one-sentence rationale per bullet).
- Numbered sections 01-10 are mandatory — section 01 is the "why now" frame, sections 02-05 are technical, section 06 is the Boardroom Playbook, section 07 is the Bank-Type segmentation, section 08 is the Roadmap, section 09 is the FAQ, section 10 is the References.
- Each article includes one Mermaid diagram in section 05.
- Each article includes two structured tables (architecture layers + security signals) in sections 02-03.
- References use the OSCOLA-style citation pattern (`Author, (Year). *Title*. Publisher. Available at: [link](url).`).

## Bencher token (separate from the posts)

The `bencher.yml` workflow already has the push steps wired (commit `59d666b`). To activate the dashboard:

1. Sign in at <https://bencher.dev> with GitHub OAuth.
2. Create a project named `euxis` (the workflow expects this exact slug — see the `--project euxis` flag in `.github/workflows/bencher.yml`).
3. Settings → Tokens → New token → copy.
4. In this repo: Settings → Secrets and variables → Actions → New repository secret → name `BENCHER_API_TOKEN`, paste the token.

The next push to `main` triggers the three Bencher uploads (`scan-bench`, `agent-bench`, `ensemble-bench`). The dashboard at <https://bencher.dev/console/projects/euxis/perf> goes live as soon as the first run completes.
