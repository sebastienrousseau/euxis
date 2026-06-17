# Slopsquatting hallucination corpus

This directory ships a **curated seed corpus** of LLM-hallucinated
package names plus a loader contract for the full **Spracklen
corpus** (205,474 unique names) published with the 2025 USENIX
Security paper.

## What is slopsquatting?

LLMs that generate code regularly emit `import` / `require` /
`use` lines for packages that do not exist. Spracklen et al.
measured 19.7 % of LLM-suggested package names across 16 models
to be non-existent. Attackers register those names on the public
registry and ship malware to anyone who copy-pastes the import.

The mitigation is deterministic: refuse to resolve a package
whose name matches a known hallucination *before* the registry
lookup. No LLM is required at scan time.

## Files

- `seed-corpus.txt` — a small representative seed shipped with
  the binary so an offline-only install still catches the highest-
  frequency hallucinations. Around 100 entries across npm, pypi,
  cargo and golang.
- `README.md` — this file.

## File format

Plain text, one entry per line.

```
# comment line (ignored)
<ecosystem>: <name>
<ecosystem>: <name> = <nearest-legitimate>
```

Recognised `<ecosystem>` values: `npm`, `pypi`, `cargo`, `golang`,
`go` (alias for `golang`), `maven`, `gem`, `generic`.

Lines without an ecosystem prefix are added to the `generic`
bucket and matched only when the scanner cannot determine the
host ecosystem from the manifest.

The optional `= <nearest-legitimate>` half is surfaced in the
resulting finding so the user can replace the hallucinated import
with the canonical one (e.g. `request = requests` for pypi).

## Refreshing from the Spracklen dataset

The full 205 K-name corpus is at
<https://github.com/Spracks/PackageHallucination>. CI workflows
should treat the seed corpus shipped here as a floor: layer the
full corpus on top by calling
`euxis::slopsquatting::Guard::load_corpus_file()` against a
download of the dataset, refreshed quarterly so newly-observed
hallucinations land in production within one release cycle.

Refresh cadence is enforced by `.github/workflows/architecture-
quality.yml` once the corpus checksum gate lands (P0.12).

## Citation

> Spracklen, J., et al. "We Have a Package for You! Comprehensive
> Analysis of Package Hallucinations by Code-Generating LLMs."
> USENIX Security 2025.
> <https://www.usenix.org/conference/usenixsecurity25/presentation/spracklen>
