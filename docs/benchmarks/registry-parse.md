# JsonCache registry-parse benchmark

The JsonCache primitive caches post-parse JSON documents in SQLite, keyed on a BLAKE2b-256 content hash. On the 53-file synthetic registry fixture it is **not** a wall-time win over direct `nlohmann::json::parse` — the hash + SQLite + msgpack overhead exceeds the parse savings for files this small. The fixture and bench harness exist so anyone can re-measure on different file-size profiles and decide whether to wire JsonCache into a given consumer.

## What this benchmark measures

Three scenarios run against the same deterministic 53-file fixture so the numbers are directly comparable.

- **`BM_ColdGetOrLoad`** opens a fresh JsonCache database every iteration and runs `get_or_load()` on the full fixture. This is what a first-invocation CLI pays — read, parse, hash, msgpack-encode, SQLite insert.
- **`BM_WarmGetOrLoad`** pre-populates the cache once and then measures the hash-validated hit path on subsequent reads. This is the steady-state cost from the second CLI invocation onward, modulo cross-process SQLite open cost.
- **`BM_DirectParse`** is the baseline JsonCache competes against — `nlohmann::json::parse` on every iteration with no cache anywhere. Establishes whether the cache primitive itself is worth its SQLite overhead.

## Methodology

The fixture mirrors the real registry shape: 41 agent docs (~1 KiB each), 6 squad docs (~600 B), 6 playbook docs (~4 KiB). Total fixture footprint is approximately 50 KiB.

Wall and CPU times come from Google Benchmark in `kMillisecond` resolution, autotuned per the harness's default convergence criteria. The bench deliberately does **not** disable filesystem caches; the cold-cache scenario reflects the realistic state where the OS page cache is warm but the JsonCache SQLite file is fresh.

A known limitation of `BM_WarmGetOrLoad`: the cache handle is shared across iterations, so the benchmark amortises the SQLite open cost across many lookups. A real cross-process invocation re-opens the cache per CLI start. The "warm" row therefore understates the cross-process steady-state cost — but the same-process measurement is still useful because it isolates the hash + SQLite SELECT + msgpack decode work that any caller pays per lookup.

## Results

Recorded 2026-06-16 on macOS / Apple Silicon (8-core) at quiet load. Numbers refresh whenever `make cpp-bench` runs against this binary.

| Scenario | Wall (ms) | CPU (ms) | Items / s | vs DirectParse |
| --- | ---: | ---: | ---: | ---: |
| `BM_DirectParse` | 3.18 | 2.02 | 26 239 | 1.00× (baseline) |
| `BM_WarmGetOrLoad` | 4.20 | 2.94 | 18 048 | 0.69× (slower) |
| `BM_ColdGetOrLoad` | 13.6 | 9.03 | 5 867 | 0.22× (slower) |

For files in this size band (≈1 KiB), the cache does **not** pay back its overhead. Direct parse is fastest because nlohmann's parser handles tiny JSON documents in microseconds, while the cache pays BLAKE2b-256 hash cost on every lookup (for invalidation), one SQLite SELECT, and one msgpack decode — three fixed costs that swamp the parse savings.

## When does JsonCache become a win?

The cache wins when **per-file parse cost exceeds (hash + SQLite + msgpack)**. That crossover is determined by file size and JSON structure depth, not by cache count. Rough rules of thumb based on the numbers above:

- **Tiny files (< 5 KiB)**: cache is a net loss. Use direct parse.
- **Medium files (5–100 KiB)**: cache is roughly break-even. Profile your actual corpus.
- **Large files (> 100 KiB) or deep JSON trees**: cache becomes a win because parse cost grows faster than hash + SQLite + msgpack overhead.

The `RegistryClient` corpus on disk is small enough that this benchmark is the honest verdict. The cache primitive earns its place in the codebase by being optionally available for callers with larger inputs (e.g., future cached SBOM documents, large playbook bundles).

## Correction to the JsonCache header doc

The original `libs/cache/include/euxis/cache/json_cache.hpp` docstring claimed the warm-cache path would be `< 5 %` of cold-cache wall time on the 53-file registry. That target is not met for the file-size profile measured here: warm is 30.9 % of cold, not 5 %. The claim should be revised to reflect the cache's actual profile (good for big files, marginal for small) in a follow-up edit to the header.

## How to reproduce

The bench builds with the rest of `libs/cache` whenever `EUXIS_BUILD_GBENCH=ON` is passed at configure time.

```sh
cmake -B build/cmake-build -S . -DEUXIS_BUILD_GBENCH=ON
cmake --build build/cmake-build --target euxis-cache-bench
build/cmake-build/libs/cache/bench/euxis-cache-bench --benchmark_min_time=0.1s
```

For a higher-confidence run (longer minimum time, JSON output suitable for trend dashboards):

```sh
build/cmake-build/libs/cache/bench/euxis-cache-bench \
    --benchmark_min_time=2s \
    --benchmark_format=json > registry-parse.json
```

To extend the fixture profile (try larger payloads), edit the `pad_bytes` arguments in `build_fixture()` inside `libs/cache/bench/bench_json_cache.cpp`. The same harness runs unchanged; only the fixture content changes.

## Why this matters

The bench file is the contract reviewers gate JsonCache changes against. Without numbers, claims about cache performance drift over time. With numbers — even unflattering ones — every PR that touches `libs/cache` has something concrete to argue with.

Refresh this table whenever the JsonCache implementation changes (msgpack version bump, hash algorithm change, SQLite pragma adjustment, etc.). A regression in any column is a real signal the team should evaluate before merging.

## See also

- [`libs/cache/include/euxis/cache/json_cache.hpp`](../../libs/cache/include/euxis/cache/json_cache.hpp) — the primitive being measured
- [`libs/cache/bench/bench_json_cache.cpp`](../../libs/cache/bench/bench_json_cache.cpp) — the bench code that produced these numbers
- [`libs/bench/README.md`](../../libs/bench/README.md) — the wider euxis benchmark story (custom + Google Benchmark harnesses)
