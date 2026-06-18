<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::taint</h1>

<p align="center">
  Forward taint analyzer over the <code>euxis::cpg</code> data-dependency
  graph. Tracks user-defined sources to sinks, respects sanitizers, emits
  <code>TaintFlow</code> records ready for SARIF or
  <code>euxis::security::Finding</code> conversion.
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
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/taint)
target_link_libraries(my_app PRIVATE euxis-taint-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/taint/analyzer.hpp"
#include "euxis/taint/builtin_specs.hpp"

int main() {
    using namespace euxis::taint;

    auto spec = builtin::cpp_web_specs();                // sources: req->*; sinks: system(), exec()
    const auto result = analyze(graph, spec);            // graph built by libs/cpg

    std::cout << "flows: " << result.flows.size() << '\n';
    for (const auto& f : result.flows) {
        std::cout << "  " << f.source_path << ":" << f.source_line
                  << " ── ▶ "
                  << f.sink_path   << ":" << f.sink_line   << '\n';
    }
}
```

## Public surface

| Header | What it owns |
|---|---|
| `spec.hpp` | `SourceSpec`, `SinkSpec`, `SanitizerSpec`, `TaintSpec` — composable rule packs |
| `builtin_specs.hpp` | Curated spec sets per language/framework (web, FFI, shell, etc.) |
| `analyzer.hpp` | `analyze(graph, spec) -> AnalysisResult`; `TaintFlow`, `AnalysisStats` |

## Examples

### Add a custom source

```cpp
#include "euxis/taint/spec.hpp"

TaintSpec spec = builtin::cpp_web_specs();
spec.sources.push_back(SourceSpec{
    .pattern = "headers->get($KEY)",
    .label   = "http-header",
});
```

### Convert flows to `euxis::security::Finding`s

```cpp
for (const auto& flow : result.flows) {
    euxis::security::Finding f;
    f.rule_id = "taint/" + flow.spec_id;
    f.severity = euxis::security::Severity::High;
    f.primary_location.path       = flow.sink_path;
    f.primary_location.start_line = flow.sink_line;
    // related_locations record the path from source to sink
}
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
