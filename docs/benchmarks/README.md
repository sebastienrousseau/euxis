# Benchmarks

This directory hosts the methodology, baseline numbers, and trend
notes for the two euxis benchmark binaries (`euxis_perf_gbench` and
`euxis_scan_gbench`).

| Document | Content |
|---|---|
| [`methodology.md`](methodology.md) | What each binary measures, why two binaries, how to run locally and in CI |
| [`baseline.md`](baseline.md) | Published reference numbers per release |

The CI workflow lives at
[`.github/workflows/bencher.yml`](../../.github/workflows/bencher.yml).
JSON output is uploaded as the `scan-bench-<os>` artifact on every
run; bencher.dev integration ships in a follow-up commit alongside
the secret-management changes.
