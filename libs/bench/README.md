<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::bench</h1>

<p align="center">
  Benchmark harnesses for euxis: a custom runner that validates against
  hardcoded SLO targets in CI, and four opt-in Google Benchmark binaries
  that feed bencher.dev / github-action-benchmark trend dashboards.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Two harnesses, two jobs](#two-harnesses-two-jobs)
- [Custom harness](#custom-harness)
- [Google Benchmark harnesses](#google-benchmark-harnesses)
- [Public surface](#public-surface)
- [Caveat](#caveat)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/bench)
target_link_libraries(my_app PRIVATE euxis-bench-cpp)
```

The Google Benchmark binaries are opt-in:

```bash
cmake -B build/cmake-build -DEUXIS_BUILD_GBENCH=ON
cmake --build build/cmake-build --target euxis_agent_gbench
```

## Two harnesses, two jobs

| Harness | Use for | Output |
|---|---|---|
| **Custom (`euxis-bench-cpp`)** | Smoke-testing all 5 suites (autonomy, interop, performance, portability, security) against hardcoded SLO targets in CI. Cheap per-bench cost (fixed wall-clock budget). | JSON via `BenchmarkResultMatrix` |
| **Google Benchmark** (4 opt-in binaries) | Statistical analysis. Auto-tunes iteration count, reports stddev and CV, integrates with bencher.dev / GitHub Actions trend dashboards. | Standard `benchmark::` text / JSON / CSV |

## Custom harness

```cpp
#include <iostream>

#include "euxis/bench/runner.hpp"

int main() {
    using namespace euxis::bench;

    BenchmarkRunner runner;
    runner.register_defaults();                          // wires the 5 default suites

    const auto reports = runner.run_all();
    for (const auto& r : reports) {
        std::cout << r.suite_name
                  << " — passed " << r.passed
                  << " / failed " << r.failed << '\n';
    }
}
```

## Google Benchmark harnesses

Four binaries, one per surface area. Each runs independently and emits bencher.dev-compatible JSON:

| Binary | What it measures |
|---|---|
| `euxis_perf_gbench`     | Crypto primitives — AES-GCM (simple + cached schedule), BLAKE2b key derivation, AgentCard msgpack round-trip |
| `euxis_scan_gbench`     | SCA parsers, SBOM emission (CycloneDX / SPDX / OpenVEX), DSSE sign/verify, slopsquatting lookup, ScanCache get/put |
| `euxis_agent_gbench`    | Agentic core — `estimate_tokens`, `WindowedContextEngine::plan`, contended `IterationBudget`, `classify_approval`, tool dispatch |
| `euxis_ensemble_gbench` | Verifier-quorum runner across 1/2/3 verifiers, finding-count Range(16..1024) |

### Running

```bash
./build/cmake-build/libs/bench/euxis_agent_gbench
./build/cmake-build/libs/bench/euxis_agent_gbench --benchmark_filter=BM_ClassifyApproval
./build/cmake-build/libs/bench/euxis_agent_gbench \
    --benchmark_format=json \
    --benchmark_out=agent-bench.json \
    --benchmark_out_format=json
```

### CI integration

The JSON output is consumed by [github-action-benchmark](https://github.com/benchmark-action/github-action-benchmark) and by bencher.dev. The `.github/workflows/bencher.yml` workflow runs all four binaries with `--benchmark_min_time=5s`, uploads the JSON as workflow artifacts, and pushes to bencher.dev when `BENCHER_API_TOKEN` is provisioned.

## Public surface

| Header | What it owns |
|---|---|
| `runner.hpp` | `BenchmarkResult`, `SuiteReport`, `BenchmarkRunner` (`register_benchmark`, `run_suite`, `run_all`, `register_defaults`) |
| `reporter.hpp` | `BenchmarkReporter` — formats `SuiteReport`s as text/JSON |
| `result_matrix.hpp` | `BenchmarkResultMatrix` — multi-suite aggregation |

## Caveat

Synthetic microbenchmarks measure *library overhead*, not user-visible performance. Before optimising a hot path that a bench identifies, profile a real `euxis check .` invocation (`perf record`, `Instruments`, or similar) and confirm the function appears in the production call graph.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
