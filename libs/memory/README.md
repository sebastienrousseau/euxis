<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::memory</h1>

<p align="center">
  Tier-bound encrypted memory storage for euxis. AES-256-GCM AEAD with
  tier-bound additional authenticated data prevents horizontal access
  across tiers and agents.
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
add_subdirectory(libs/memory)
target_link_libraries(my_app PRIVATE euxis-memory-cpp)
```

## Quick start

```cpp
#include <iostream>
#include <vector>

#include "euxis/memory/store.hpp"
#include "euxis/memory/tier.hpp"

int main() {
    using namespace euxis::memory;

    std::array<std::byte, 32> key{};                    // from libs/crypto
    EncryptedMemoryStore store{key};

    EncryptedMemoryEntry e;
    e.agent_id = "scan-agent";
    e.tier     = MemoryTier::Hot;
    e.payload  = /* serialise your memory entry */ {};

    auto rc = store.put("entry-001", e);
    if (!rc) std::cerr << "put failed\n";

    auto got = store.get("entry-001", "scan-agent", MemoryTier::Hot);
    std::cout << "round-trip: " << got.has_value() << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `tier.hpp` | `MemoryTier` enum (Hot, Warm, Cold) |
| `entry.hpp` | `EncryptedMemoryEntry` — record + per-entry IV |
| `store.hpp` | `EncryptedMemoryStore` — AEAD store; AAD = `(agent_id, tier)` so cross-tier or cross-agent reads decrypt to nothing |

## Examples

### Tier promotion (move an entry from Hot to Warm)

```cpp
auto hot_entry = store.get("entry-001", "scan-agent", MemoryTier::Hot);
if (hot_entry) {
    auto warm = *hot_entry;
    warm.tier = MemoryTier::Warm;
    store.put("entry-001", warm);                       // re-encrypts with new AAD
    store.remove("entry-001", "scan-agent", MemoryTier::Hot);
}
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
