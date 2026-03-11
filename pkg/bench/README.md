# Euxis Bench C++

The `euxis::bench` module enforces the "Outcome Perfect" benchmark tracking for the Agent OS. It provides precise, high-resolution validation against the ZeroClaw and OpenClaw standards.

## Continuous Performance Validation

The `Runner` orchestrates hardware-native microbenchmarks.

* **Precondition**: Ensure test suites are isolated from the host OS scheduler noise.
* **Postcondition**: Synthesizes structured JSON profiling output for pipeline consumption.

For extreme resolution, use `std::chrono::steady_clock` to mitigate integer rounding in sub-millisecond ranges. The benchmarker relies on cache-warmed paths to measure zero-copy C++23 implementations cleanly.

## Matrix Validation

The `ResultMatrix` aggregates metrics across autonomy, interop, portability, and security axes.

* **SoA**: Structure of Arrays — Flat memory tracking for optimized scoring.
* **std::span**: Bounds-checked memory view — Safe vector ingestion.

```cpp
auto res = benchmark.run_all(filter_span)
    .and_then([](auto&& results) { return assert_bounds(results); })
    .or_else([](auto&& err) { return halt_pipeline(err); });
```

Utilize C++23 monadic operations to guarantee that any benchmark failing the baseline (e.g., memory residency > 5MB, or routing > 10ms) immediately fails the pipeline execution.