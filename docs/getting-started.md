# Getting started — first scan in ten minutes

A guided tour from a fresh install to a real evidence-grade scan. Each
step shows the actual command, the actual output, and what to read in
that output.

If `euxis` isn't installed yet, follow
[`docs/essentials/quick-start.md`](essentials/quick-start.md) first
(clone → build → symlink, ~5 minutes), then come back here.

---

## 1. Sanity-check the install — 30 seconds

```bash
euxis doctor
```

`doctor` checks platform, required tools, data directories, and the
agent registry. Expected output is a column of green ticks:

```
Euxis Doctor

System:
  Hostname:     <yours>
  Kernel:       <yours>
  Architecture: arm64

Checks:
  ✔ Platform (macOS)
  ✔ git (found)
  ✔ sqlite3 (found)
  ✔ jq (found)
  ✔ EUXIS_HOME (/Users/<you>/.euxis)
  ✔ dir: data/agents (ok)
  ✔ registry.json (ok)
  ✘ dir: data/runtime/memory (missing)         ← created on first run
```

A handful of `data/runtime/*` paths show as missing on a fresh install
— they are created lazily when the first session needs them. Anything
under **Checks** that is red and **not** `data/runtime/...` is a
real problem; the [troubleshooting guide](essentials/quick-start.md#troubleshooting)
covers the common ones.

**Exit code 0** means doctor was happy enough to continue.

---

## 2. First real result — query a known-vulnerable package — 5 seconds

This is the fastest "I see real value" path. `vulndb query` takes a
[Package URL](https://github.com/package-url/purl-spec) and asks
OSV.dev for everything it knows about that exact version:

```bash
euxis vulndb query "pkg:npm/lodash@4.17.20"
```

```
OSV lookup for pkg:npm/lodash@4.17.20 — 5 vulnerabilities reported
  GHSA-29mw-wpgm-hmr9  [unknown / cvss 0]
    Regular Expression Denial of Service (ReDoS) in lodash
    Fixed in: 4.17.21
  GHSA-35jh-r3h4-6jhm  [unknown / cvss 0]
    Command Injection in lodash
    Fixed in: 4.17.21
  GHSA-f23m-r3pf-42rh  [unknown / cvss 0]
    lodash vulnerable to Prototype Pollution via array path bypass …
    Fixed in: 4.18.0
  …
```

Five GHSAs, each with a fix version. The **exit code** is the load-bearing
signal:

| Exit | Meaning |
|---|---|
| 0   | No findings or only informational |
| 2   | Advisory findings (medium or low severity) |
| 3   | Blocking findings (high or critical) — CI should fail |
| 1   | Infrastructure problem (network, OSV.dev down) |

Wire `vulndb query` into a release-gate step and the exit code is
your entire policy. Add `--json` for machine-readable output.

---

## 3. Inventory your supply chain — 10 seconds

`sbom` walks the target directory, recognises ecosystem lockfiles
(Cargo, npm, pip, Go modules), and emits a canonical
[CycloneDX 1.6](https://cyclonedx.org/) document:

```bash
euxis sbom .
```

```
euxis sbom: wrote euxis-sbom.cdx.json
euxis sbom: scanned 54 manifest(s), 523 component(s)
```

The CycloneDX file is the standard SBOM format your auditors and
downstream consumers will already understand. A component entry looks
like this:

```json
{
  "bom-ref": "pkg:cargo/anyhow@1.0.102",
  "name": "anyhow",
  "purl": "pkg:cargo/anyhow@1.0.102",
  "version": "1.0.102",
  "type": "library",
  "hashes": [{ "alg": "SHA-256", "content": "7f202df8…" }]
}
```

Other formats:

```bash
euxis sbom . --format=spdx          # SPDX 3.0.1
euxis sbom . --format=both          # CycloneDX + SPDX side by side
euxis sbom . --format=openvex       # empty VEX skeleton
```

---

## 4. Cross-reference SBOM against OSV.dev — 30 seconds

This is the supply-chain payoff. Each component in the SBOM is looked
up in OSV.dev and any vulnerabilities are emitted as
[OpenVEX](https://openvex.dev/) statements:

```bash
euxis sbom . --format=openvex --enrich
```

```
euxis sbom: enriched 523 component(s); 18 VEX statement(s)
euxis sbom: wrote euxis-vex.openvex.json
```

`18 VEX statement(s)` means OSV.dev knew about 18 distinct
vulnerabilities across the 523 components. The OpenVEX file is what
you ship alongside the SBOM in a release artifact — it lets a
downstream consumer prove a known CVE does or does not affect your
build.

---

## 5. Multi-agent verification — 1-2 minutes

The verbs in §2-§4 are deterministic and need no AI provider. The
multi-agent verdict layer needs at least one provider. The cheapest
setup is Ollama running locally:

```bash
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull qwen2.5-coder:7b
export EUXIS_LOCAL_ONLY=1
```

Then trigger a triage scan:

```bash
euxis triage .
```

`triage` deploys two agents in flash mode (45-75-second budget). The
output is a five-part verdict:

```
=== CONCURRENT EVIDENCE COLLECTION ===
  check  (qwen2.5-coder:7b)
    ✔ check [PASS] 10.0ms | 0 exec/1 inferred
  …

  --- Evidence Pillars ---
    security:     Verified  (cov:100% agr:100% depth:100%)
    testing:      Verified  (cov:100% agr:100% depth:100%)
    docs:         Issue     (cov:100% agr:100% depth:0%)
    architecture: Issue     (cov:50% agr:100% depth:0%)
    build:        Issue     (cov:50% agr:100% depth:0%)

  --- Verdict Trend ---
    Trend:    = stable
    History:  2 runs
    Delta:    +0 confidence vs last run (BLOCKED → BLOCKED)

  --- Benchmark ---
    Total:    267.9ms
    Agents:   12 executed
    Cost:     $0.0000
    Context:  Cloud + Local
```

What to read:

- **Evidence Pillars** — seven dimensions (security, testing, docs,
  architecture, build, execution, truth) each scored independently.
  `Verified` = enough independent agents agreed; `Issue` = gap.
- **Verdict Trend** — what's the delta against the previous run on
  this target. Stops you mistaking "always BLOCKED" for "regression".
- **Benchmark** — wall time, agent count, cost. Triage is bounded
  here; `check` and `review --forensic` are the deeper modes.

If you want the structured verdict for CI, the JSON lives at
`data/runtime/sessions/latest_verdict.json`.

---

## 6. Where to go from here

| If you want… | Read |
|---|---|
| Every command and flag | [`docs/reference/cli-reference.md`](reference/cli-reference.md) |
| The agent roster + provider routing | [`docs/guides/fleet-guide.md`](guides/fleet-guide.md) |
| Embedding euxis as a C++ library | [`docs/examples/cpp/a2a_minimal_server/`](examples/cpp/a2a_minimal_server/) |
| The SARIF / CycloneDX / OpenVEX wire format | [`docs/architecture.md`](architecture.md) |
| CI integration (GitHub Actions) | [`.github/actions/euxis-scan/action.yml`](../.github/actions/euxis-scan/action.yml) |
| Why each finding maps to a CWE | [`docs/reference/cli-reference.md`](reference/cli-reference.md) |

---

## TL;DR

```bash
euxis doctor                                     # sanity-check install
euxis vulndb query "pkg:npm/lodash@4.17.20"      # OSV.dev lookup
euxis sbom . --format=openvex --enrich           # SBOM + auto-VEX
euxis triage .                                   # flash multi-agent verdict
```

Four commands, two evidence-grade artifacts on disk
(`euxis-sbom.cdx.json`, `euxis-vex.openvex.json`), one verdict.
