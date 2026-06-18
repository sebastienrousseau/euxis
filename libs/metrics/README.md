<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::metrics</h1>

<p align="center">
  Observability surface for euxis: low-overhead task/tool/delegation
  counters, per-session insights with provider pricing, evidence
  framework for citation-checking, and an mdspan-backed agent metrics grid.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — record a turn, aggregate session insights
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/metrics)
target_link_libraries(my_app PRIVATE euxis-metrics-cpp)
```

## Quick start

```cpp
#include <iostream>
#include <vector>

#include "euxis/metrics/insights.hpp"

int main() {
    using namespace euxis::metrics;

    const auto pricing = lookup_pricing("anthropic", "claude-haiku-4-5");

    UsageRecord rec;
    rec.session_id     = "s-001";
    rec.agent_id       = "scan-agent";
    rec.provider       = "anthropic";
    rec.model          = "claude-haiku-4-5";
    rec.input_tokens   = 1200;
    rec.output_tokens  = 320;
    rec.cost_usd       = estimate_cost(pricing, rec.input_tokens, /*cached=*/0, rec.output_tokens);

    const auto insights = aggregate("s-001", std::vector{rec});
    std::cout << "total cost: $" << insights.total_cost_usd << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `fast_collector.hpp` | `FastMetricsCollector`, `FastMetricsBuffer`, `MetricEvent` — lock-free-ish ring buffer for the hot path |
| `performance_collector.hpp` | `PerformanceMetricsCollector` + per-event contexts (`TaskStartContext`, `TaskCompletionContext`, `TaskFailureContext`, `DelegationContext`, `ToolExecutionContext`) |
| `analyzer.hpp` | `PerformanceAnalyzer` — derives p50/p95/p99 and trend deltas from the collector output |
| `insights.hpp` | `UsageRecord`, `ProviderPricing`, `SessionInsights`; `lookup_pricing(provider, model)`, `estimate_cost(...)`, `aggregate(session_id, records)` |
| `evidence.hpp` | `Evidence`, `Claim`, `EvidenceGrade`, `EvidenceFramework` — grade-and-cite framework for agent assertions |
| `validation_pipeline.hpp` | `ExtractedClaim`, `CitationCheckResult`, `ValidationResult`, `ValidationPipeline` |
| `mdspan_views.hpp` | `AgentMetricsGrid` — C++23 `std::mdspan`-backed flat-array grid for multi-agent dashboards |
| `types.hpp` | Shared aliases |

## Examples

### Hot-path event capture

```cpp
#include "euxis/metrics/fast_collector.hpp"

FastMetricsCollector fmc;
fmc.record(MetricEvent{
    .kind      = MetricEvent::Kind::ToolDispatched,
    .timestamp = std::chrono::steady_clock::now(),
    .agent_id  = "scan-agent",
});
```

### Multi-agent dashboard via mdspan

```cpp
#include "euxis/metrics/mdspan_views.hpp"

AgentMetricsGrid grid{/*agents=*/16, /*metrics=*/8};
grid[agent_idx, metric_idx] = 1.42;                    // C++23 multidim subscript
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
