<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::slopsquatting</h1>

<p align="center">
  Typosquat / hallucinated-package detector. Catches LLM-fabricated
  dependency names by checking against a known-good corpus. Implements
  the slopsquatting attack defence from Spracklen et al., USENIX 2025.
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
- [Background](#background)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/slopsquatting)
target_link_libraries(my_app PRIVATE euxis-slopsquatting-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/slopsquatting/guard.hpp"

int main() {
    using namespace euxis::slopsquatting;

    Guard guard;
    auto loaded = guard.load_corpus("./data/slopsquatting/pypi.csv");
    if (!loaded) { std::cerr << "corpus load failed\n"; return 1; }

    // Suspect dependency emitted by an LLM agent.
    const auto match = guard.check("requets");          // typo of "requests"
    if (match.is_suspicious) {
        std::cout << "BLOCKED: " << match.candidate
                  << " (nearest: " << match.nearest_known
                  << ", distance: " << match.edit_distance << ")\n";
    }
}
```

## Public surface

| Header | What it owns |
|---|---|
| `guard.hpp` | `Guard`, `Match`, `CorpusError` |
| `integration.hpp` | Helpers for wiring the guard into the `libs/sca` scan path |

## Background

LLM-driven coding agents occasionally invent plausible-but-fake package names. Slopsquatting attackers register those exact names so the next agent that hallucinates the same name pulls in attacker-controlled code. The guard checks every suggested dependency against a curated corpus and flags edit-distance-close candidates that aren't in the corpus — the canonical signature of a hallucination.

Reference: Spracklen, S. et al., *"We have a package for you!"*, USENIX Security 2025.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
