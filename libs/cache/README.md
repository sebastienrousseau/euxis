<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::cache</h1>

<p align="center">
  Content-hashed cache primitives for euxis: a fast in-process
  <code>JsonCache</code>, a libsodium-protected on-disk <code>ScanCache</code>,
  and a content-addressable <code>Hasher</code> built on BLAKE2b.
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
add_subdirectory(libs/cache)
target_link_libraries(my_app PRIVATE euxis-cache-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/cache/json_cache.hpp"

int main() {
    using namespace euxis::cache;

    JsonCache cache;

    KeyInputs k;
    k.path    = "data/agents/registry.json";
    k.content = read_file_bytes(k.path);                 // your loader

    if (auto hit = cache.get(k)) {
        std::cout << "hit: " << hit->dump().substr(0, 50) << '\n';
    } else {
        auto parsed = nlohmann::json::parse(k.content);
        cache.put(k, parsed);
    }

    const auto stats = cache.stats();
    std::cout << "hits=" << stats.hits << " misses=" << stats.misses << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `key.hpp` | `KeyInputs` — `(path, content)` tuple used to compute content-hashed cache keys; `CacheEntry` |
| `hash.hpp` | `Hasher` (BLAKE2b backend), `HashError` |
| `json_cache.hpp` | `JsonCache` (in-process, thread-safe) + `JsonCacheStats` |
| `store.hpp` | `ScanCache` — libsodium-encrypted on-disk cache for scan results, with SQLite-backed storage; `CacheStats`, `StoreError` |

## Examples

### Avoid re-parsing the agent registry on every CLI invocation

```cpp
#include "euxis/cache/json_cache.hpp"

JsonCache registry_cache;

auto load_registry = [&] {
    KeyInputs k{ .path = "data/agents/registry.json",
                 .content = read_file_bytes(k.path) };
    if (auto hit = registry_cache.get(k)) return *hit;
    auto j = nlohmann::json::parse(k.content);
    registry_cache.put(k, j);
    return j;
};
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
