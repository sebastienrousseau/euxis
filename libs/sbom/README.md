<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::sbom</h1>

<p align="center">
  Software Bill of Materials emitters for euxis: CycloneDX 1.6,
  SPDX 3.0.1, OpenVEX, plus a Package URL parser for cross-format
  component identity.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — emit one component as CycloneDX
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/sbom)
target_link_libraries(my_app PRIVATE euxis-sbom-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/sbom/types.hpp"
#include "euxis/sbom/cyclonedx.hpp"

int main() {
    using namespace euxis::sbom;

    SbomDocument doc;
    doc.components.push_back(Component{
        .type     = ComponentType::Library,
        .name     = "libsodium",
        .version  = "1.0.20",
        .purl     = "pkg:generic/libsodium@1.0.20",
        .licenses = {License{.id = "ISC"}},
        .hashes   = {Hash{.algorithm = HashAlgorithm::Sha256, .value = "..."}},
    });

    const auto json = emit_cyclonedx(doc);              // CycloneDX 1.6
    std::cout << json.dump(2) << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `types.hpp` | `Component`, `Dependency`, `License`, `Hash` (+ algorithm enum), `ExternalRef` (+ type enum), `Tool`, `SbomDocument`, `ComponentType` |
| `cyclonedx.hpp` | `emit_cyclonedx(doc)` — CycloneDX 1.6 JSON |
| `spdx.hpp` | `emit_spdx(doc)` — SPDX 3.0.1 JSON-LD |
| `openvex.hpp` | `VexDocument`, `VexStatement`, `VexStatus`, `VexJustification`, emitters |
| `purl.hpp` | `Purl`, `PurlType`, `PurlError` — Package URL spec parser |

## Examples

### Emit an OpenVEX `not_affected` statement

```cpp
#include "euxis/sbom/openvex.hpp"

VexDocument vex;
vex.statements.push_back(VexStatement{
    .vulnerability  = "CVE-2026-XXXXX",
    .status         = VexStatus::NotAffected,
    .justification  = VexJustification::ComponentNotPresent,
    .product_ids    = {"pkg:generic/myapp@1.0.0"},
});
const auto json = emit_openvex(vex);
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
