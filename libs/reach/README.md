<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::reach</h1>

<p align="center">
  Callgraph construction and reachability analysis for euxis. Promotes
  intra-procedural CPGs to a whole-program callgraph and answers
  "which findings are actually reachable from a public entry point?".
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start)
- [Public surface](#public-surface)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/reach)
target_link_libraries(my_app PRIVATE euxis-reach-cpp)
```

## Quick start

```cpp
#include <iostream>
#include <vector>

#include "euxis/reach/callgraph.hpp"
#include "euxis/reach/reachability.hpp"

int main() {
    using namespace euxis::reach;

    // Build a callgraph from a collection of intra-procedural CPGs.
    const auto cg = build_callgraph(graphs);             // CallGraphResult

    // Probe reachability from the public entry points.
    ReachabilityConfig cfg;
    cfg.entry_points = {"main", "handler::*"};
    cfg.max_depth    = 50;

    const auto result = compute_reachability(cg.graph, cfg);
    std::cout << "reachable functions: " << result.reachable.size() << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `callgraph.hpp` | `CallEdge`, `CallGraph`, `CallGraphStats`, `CallGraphResult`; `build_callgraph(graphs)` |
| `reachability.hpp` | `ReachabilityConfig`, `ReachabilityResult`; `compute_reachability(callgraph, cfg)` |

This module is the inter-procedural layer above `libs/cpg`. Pair it with `libs/taint` to filter taint flows down to "reachable from any public entry" — the classic SAST false-positive reduction step.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
