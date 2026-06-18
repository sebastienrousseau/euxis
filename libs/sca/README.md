<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::sca</h1>

<p align="center">
  Software Composition Analysis for euxis. Parses lockfiles across four
  ecosystems (Cargo, Go modules, npm, pip) and exposes one normalised
  <code>ParsedManifest</code> the SBOM stack consumes.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — parse `Cargo.lock`
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/sca)
target_link_libraries(my_app PRIVATE euxis-sca-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/sca/cargo_lock.hpp"

int main() {
    using namespace euxis::sca;

    auto parsed = parse_cargo_lock_file("./Cargo.lock");
    if (!parsed) {
        std::cerr << "parse failed: " << parsed.error().message << '\n';
        return 1;
    }
    std::cout << "ecosystem: cargo\n"
              << "entries:   " << parsed->entries.size() << '\n';
    for (const auto& e : parsed->entries) {
        std::cout << "  " << e.name << "@" << e.version << '\n';
    }
}
```

## Public surface

| Header | What it owns |
|---|---|
| `manifest.hpp` | `Ecosystem` enum, `ManifestEntry`, `ParsedManifest`, `ParseError` |
| `cargo_lock.hpp` | `parse_cargo_lock` (string), `parse_cargo_lock_file` (path) |
| `go_sum.hpp` | `parse_go_sum`, `parse_go_sum_file` |
| `npm_lock.hpp` | `parse_npm_lock`, `parse_npm_lock_file` — handles npm v1, v2, v3 lockfile shapes |
| `pipfile_lock.hpp` | `parse_pipfile_lock`, `parse_pipfile_lock_file` |
| `scanner.hpp` | `ScanOptions`, `ScanResult` — orchestrates ecosystem auto-detection across a directory tree |

## Examples

### Auto-detect ecosystem and scan a directory

```cpp
#include "euxis/sca/scanner.hpp"

ScanOptions opts;
opts.root = "./my-monorepo";
opts.recursive = true;

const auto result = scan(opts);
for (const auto& [eco, manifest] : result.manifests) {
    std::cout << static_cast<int>(eco) << " " << manifest.entries.size() << '\n';
}
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
