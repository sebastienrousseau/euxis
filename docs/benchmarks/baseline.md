# Baseline numbers

This page tracks the most-recently-published benchmark numbers, one
table per release. The shape of each table matches the
`benchmarks[]` array in Google Benchmark JSON output, so the entries
can be diffed mechanically.

## v0.1.0-beta (target, not yet measured)

The numbers below are the **targets** from the [v0.0.3 implementation
plan §10](../../../Drop/euxis-ip.md). They will be replaced with the
measured values on the v0.1.0-beta CI run.

| Benchmark | Target (`real_time`) | Floor | Notes |
|---|---:|---:|---|
| `BM_CycloneDX_Emit_5K`     | < 50 ms | < 200 ms | 5K-component synthetic doc |
| `BM_SPDX_Emit_5K`          | < 80 ms | < 250 ms | Same fixture |
| `BM_OpenVEX_Emit_100`      | < 200 µs | < 1 ms   | 100 statements |
| `BM_CargoLock_Parse_1K`    | < 10 ms | < 50 ms  | 1K `[[package]]` blocks |
| `BM_NpmLock_Parse_1K`      | < 15 ms | < 60 ms  | 1K node_modules packages |
| `BM_Slopsq_Lookup`         | < 200 ns | < 1 µs   | Hash-set lookup against seed corpus |
| `BM_DSSE_Sign`             | < 100 µs | < 500 µs | One Ed25519 signature |
| `BM_DSSE_Verify`           | < 100 µs | < 500 µs | One Ed25519 verify |
| `BM_Cache_PutGet`          | < 200 µs | < 1 ms   | SQLite WAL-mode cache |
| `BM_Hash_File_64KiB`       | < 200 µs | < 1 ms   | BLAKE2b via libsodium |

End-to-end SBOM-to-bundle latency (CycloneDX 1.6 + SPDX 3.0.1 +
OpenVEX + DSSE sign on a 5K-dep monorepo) should land **under 30
seconds** on a stock GitHub Actions `ubuntu-24.04` runner per the
P0 KPI table. The composition is mechanical:

    BM_CycloneDX_Emit_5K + BM_SPDX_Emit_5K + BM_OpenVEX_Emit_100 + BM_DSSE_Sign
    ≈ 50ms + 80ms + 200µs + 100µs ≈ 130 ms

leaving 99.9 % of the 30-second budget for I/O, manifest discovery,
and OSV.dev / NVD database lookup once those land.

## How this file is updated

A future CI step will append a measured-numbers row per release tag,
sourced from `artifacts/scan-bench.json` of the green release
workflow. Until that automation lands, this file is updated by hand
at tag-cut time.
