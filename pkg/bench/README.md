# euxis-bench-cpp

C++23 benchmark suite — security, autonomy, performance, portability, and interop benchmarks.

## Overview

euxis-bench-cpp provides a comprehensive benchmark runner for the Euxis C++ stack. It exercises five suites: security (malicious skill detection, sandbox escape resistance), autonomy (multi-step task completion), performance (crypto throughput, KDF latency, skill import rate), portability (cross-platform build verification), and interop (A2A protocol round-trips). Results are reported in JSON and Markdown formats. Targets include >99.5% malicious detection, 100% sandbox escape resistance, >50k crypto ops/sec, <5ms KDF P95, and >10k skill imports/sec.

## Dependencies

- All euxis-cpp modules (euxis-crypto-cpp, euxis-bridge-cpp, euxis-memory-cpp, euxis-identity-cpp, euxis-inference-cpp, euxis-a2a-cpp)
- nlohmann-json
- spdlog

## Building

```bash
# From the euxis-cpp root
cmake -B build -S .
cmake --build build --target euxis-bench-cpp
```

## Testing

```bash
ctest --test-dir build -R euxis-bench-cpp_tests
```

## API

- **runner.hpp** -- BenchmarkRunner: suite registration, execution, and result aggregation.
- **reporter.hpp** -- JSON and Markdown report generation from benchmark results.
