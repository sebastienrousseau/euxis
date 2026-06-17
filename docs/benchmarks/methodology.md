# Benchmark methodology

Euxis ships **two** benchmark binaries. Each one answers a different
question, and the two together back the public performance numbers in
the README.

| Binary | What it measures | Built when |
|---|---|---|
| `euxis_perf_gbench` | Crypto primitives (AES-GCM throughput, BLAKE2b key derivation), A2A msgpack round-trip | `-DEUXIS_BUILD_GBENCH=ON` |
| `euxis_scan_gbench` | The user-visible evidence pipeline: SBOM emission, SCA parsing, DSSE sign/verify, slopsquatting lookup, scan-cache get/put, file hashing | `-DEUXIS_BUILD_GBENCH=ON` |

Both binaries emit Google Benchmark JSON (`--benchmark_format=json`).
That format is consumed directly by [bencher.dev](https://bencher.dev),
[github-action-benchmark](https://github.com/benchmark-action/github-action-benchmark),
and any in-house dashboard that speaks the standard schema.

## Why two binaries

The crypto primitives turn over in the nanosecond range and need
`--benchmark_min_time=0.1s` to converge. The scan + evidence
benchmarks turn over in microseconds-to-milliseconds and need
`--benchmark_min_time=5s` to get tight error bars on the slower
cases (SBOM emission on a 5,000-component document, for example).
Running both at the higher of the two budgets wastes CI time;
running both at the lower of the two undercounts the scan numbers.
Separating them lets each run at its natural pace.

## What `euxis_scan_gbench` covers

| Benchmark | Fixture | Unit |
|---|---|---|
| `BM_CycloneDX_Emit_5K` | Synthetic 5,000-component SbomDocument | ms |
| `BM_SPDX_Emit_5K` | Same fixture | ms |
| `BM_OpenVEX_Emit_100` | 100 not-affected VEX statements | µs |
| `BM_CargoLock_Parse_1K` | 1,000 `[[package]]` blocks | ms |
| `BM_NpmLock_Parse_1K` | 1,000 `node_modules/*` packages | ms |
| `BM_Slopsq_Lookup` | Embedded seed corpus | ns |
| `BM_DSSE_Sign` | Minimal in-toto Statement | µs |
| `BM_DSSE_Verify` | Same | µs |
| `BM_Cache_PutGet` | SQLite scan cache, fresh DB | µs |
| `BM_Hash_File_64KiB` | 64 KiB blob (typical source file) | µs |

All fixtures are built in-process — no on-disk dependencies beyond
the temp-file-scoped SQLite cache. This makes runs deterministic
and CI-resumable.

## What's deliberately NOT here yet

The KPI table in [the v0.0.3 implementation
plan](../../../Drop/euxis-ip.md) calls for:

- `BM_ScanThroughput_C_1M_LOC` — synthetic 1M-LOC C source
- `BM_ScanRust_TokioRepo` — pinned tokio clone
- `BM_ScanGo_KubernetesSubset` — pinned k8s subset
- `BM_ScanIncremental_OneFileChange` — warm-cache delta

These benchmarks measure the work that **the SAST engine does**, and
the SAST engine itself (tree-sitter parser, Code Property Graph,
taint analysis) is P1. Adding them now would either measure
synthetic work that has no relationship to the real pipeline or
require pinning a fixture corpus that drifts between releases.

The scan_bench.cpp suite ships the benchmarks we can run honestly
**today**, with the right shape (Google Benchmark JSON) so they
pair-up cleanly with the P1 additions when those land.

## Running locally

```bash
# Configure with the gbench target enabled.
cmake -B cmake-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DEUXIS_BUILD_GBENCH=ON

# Build only the scan-bench target.
cmake --build cmake-build --parallel --target euxis_scan_gbench

# Run with the same parameters CI uses.
./cmake-build/libs/bench/euxis_scan_gbench \
  --benchmark_min_time=5s \
  --benchmark_format=json \
  --benchmark_out=scan-bench.json \
  --benchmark_out_format=json
```

A human-friendly run is fast:

```bash
./cmake-build/libs/bench/euxis_scan_gbench --benchmark_min_time=0.5s
```

## CI integration

`.github/workflows/bencher.yml` runs `euxis_scan_gbench` on every
push to `main` and on PRs that touch the relevant library
directories. The JSON output is attached as the
`scan-bench-<os>` workflow artifact and retained for 90 days.

A follow-up commit will wire the artifact to a real bencher.dev
project so trend graphs land on every push automatically; that
needs the bencher.dev API token, which lives in repository secrets
and is therefore outside the scope of this PR.

## Reproducibility

Every benchmark in scan_bench.cpp is deterministic:

- Synthetic fixtures are built from `std::to_string(i)`-style loops,
  no clocks, no `std::random` seeded from `random_device`.
- The DSSE sign benchmark generates a keypair once at fixture setup
  and reuses it across iterations.
- The scan-cache benchmark opens a tempfile-scoped SQLite database
  per run and cleans it up on exit.
- Google Benchmark itself reports the iteration count, mean, median,
  stddev, and CV per case, so the JSON is enough to detect drift
  without re-running.

This means you can re-run the bench harness on the same SHA, on the
same host, with the same `--benchmark_min_time`, and expect numbers
that match to ~1 % CV.

## Reading the JSON output

Each benchmark produces an entry under `benchmarks[]` with this
shape (abbreviated):

```json
{
  "name": "BM_CycloneDX_Emit_5K",
  "iterations": 4321,
  "real_time": 1.234,
  "cpu_time": 1.220,
  "time_unit": "ms",
  "items_per_second": 4.06e6,
  "bytes_per_second": 0
}
```

- `real_time` is what humans care about — wall clock per iteration.
- `cpu_time` should track `real_time` closely for these benchmarks
  (they're not I/O-bound except for the file-hash one).
- `items_per_second` is set where the benchmark reports an item
  count via `state.SetItemsProcessed()` — useful for relative
  comparisons across changes that affect per-component work.
- `bytes_per_second` is set on the SCA parser benchmarks (input
  bytes consumed per second) and the file-hash benchmark.

The schema is documented exhaustively at
https://github.com/google/benchmark/blob/main/docs/user_guide.md.
