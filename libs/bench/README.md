# Euxis Bench C++

The `euxis::bench` module provides benchmarking for the Agent OS across two
complementary harnesses.

## Two harnesses, two jobs

| Harness | Use for | Output |
|---|---|---|
| **Custom (`euxis-bench-cpp`)** | Smoke-testing all 5 suites (autonomy, interop, performance, portability, security) against hardcoded SLO targets in CI. Cheap per-bench cost (fixed wall-clock budget). | JSON via `ResultMatrix` |
| **Google Benchmark (`euxis_perf_gbench`)** | Statistical analysis of the `performance` suite. Auto-tunes iteration count, reports stddev and CV, integrates with bencher.dev / GitHub Actions trend dashboards. Slower per-run but rigorous. | Standard `benchmark::` text/JSON/CSV/console |

Both harnesses link the same underlying `libs/crypto` and `libs/a2a` code, so
their numbers should agree on a quiet machine.

## Custom harness — continuous performance validation

The `Runner` orchestrates hardware-native microbenchmarks.

* **Precondition**: Ensure test suites are isolated from the host OS scheduler noise.
* **Postcondition**: Synthesizes structured JSON profiling output for pipeline consumption.

For extreme resolution, use `std::chrono::steady_clock` to mitigate integer rounding in sub-millisecond ranges. The benchmarker relies on cache-warmed paths to measure zero-copy C++23 implementations cleanly.

### Matrix Validation

The `ResultMatrix` aggregates metrics across autonomy, interop, portability, and security axes.

* **SoA**: Structure of Arrays — Flat memory tracking for optimized scoring.
* **std::span**: Bounds-checked memory view — Safe vector ingestion.

```cpp
auto res = benchmark.run_all(filter_span)
    .and_then([](auto&& results) { return assert_bounds(results); })
    .or_else([](auto&& err) { return halt_pipeline(err); });
```

Utilize C++23 monadic operations to guarantee that any benchmark failing the baseline (e.g., memory residency > 5MB, or routing > 10ms) immediately fails the pipeline execution.

## Google Benchmark harness

Built only when `-DEUXIS_BUILD_GBENCH=ON` is passed at configure time. The
`euxis_perf_gbench` binary mirrors `performance_bench.cpp` 1:1 (four
benchmarks: AES-256-GCM throughput simple + cached, BLAKE2b key derivation,
agent-card msgpack round-trip) so results cross-validate against the custom
harness.

### Building

```bash
cmake -B build/cmake-build \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DEUXIS_BUILD_GBENCH=ON
cmake --build build/cmake-build --target euxis_perf_gbench
```

### Running

```bash
# Default text output
./build/cmake-build/libs/bench/euxis_perf_gbench

# Filter to a single benchmark
./build/cmake-build/libs/bench/euxis_perf_gbench \
  --benchmark_filter=BM_CryptoThroughput_Cached

# JSON output for trend tracking
./build/cmake-build/libs/bench/euxis_perf_gbench \
  --benchmark_format=json \
  --benchmark_out=perf.json --benchmark_out_format=json

# Tighter statistical bounds (10 repetitions, drop outliers)
./build/cmake-build/libs/bench/euxis_perf_gbench \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true
```

### CI integration

The JSON output is consumed directly by
[github-action-benchmark](https://github.com/benchmark-action/github-action-benchmark).
Wire it into a workflow step that runs on `main` and stores results in
`gh-pages` — automatic regression alerts on PRs.

## Caveat

Synthetic microbenchmarks measure *library overhead*, not user-visible
performance. Before optimizing a hot path that a bench identifies, profile
a real `euxis-cli` workload (`perf record euxis-cli triage .`) and confirm
the function appears in the production call graph.
