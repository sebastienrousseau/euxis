<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::vulndb</h1>

<p align="center">
  Vulnerability enrichment for euxis SBOMs. Cross-references
  <code>SbomDocument::components</code> against the OSV.dev REST API
  and auto-emits OpenVEX statements for every affected (or, optionally,
  every clean) component.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — enrich a CycloneDX SBOM in twelve lines
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/vulndb)
target_link_libraries(my_app PRIVATE euxis-vulndb-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/sbom/cyclonedx.hpp"
#include "euxis/sbom/types.hpp"
#include "euxis/vulndb/enricher.hpp"
#include "euxis/vulndb/osv_client.hpp"

int main() {
    using namespace euxis;
    using namespace euxis::vulndb;

    sbom::SbomDocument doc;
    doc.components.push_back(sbom::Component{
        .type    = sbom::ComponentType::Library,
        .name    = "requests",
        .version = "2.20.0",
        .purl    = "pkg:pypi/requests@2.20.0",
    });

    OsvClient client;                                       // hits api.osv.dev
    Enricher enricher{client, EnricherConfig{.emit_not_affected = false}};

    auto report = enricher.enrich(doc);
    if (!report) {
        std::cerr << "enrich failed: " << report.error().message << '\n';
        return 1;
    }
    std::cout << "matches: " << report->matches.size()
              << "  affected statements: " << report->vex.statements.size()
              << "  has_high_or_critical: " << report->has_high_or_critical()
              << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `types.hpp`      | `OsvVuln`, `VulnMatch`, `EnrichedReport`, `Severity`, `severity_from_cvss`, `severity_name` |
| `errors.hpp`     | `ErrorKind`, `Error`, `error_name` |
| `osv_client.hpp` | `OsvClient`, `OsvClientConfig`, `query_by_purl`, `query_by_package` |
| `enricher.hpp`   | `Enricher`, `EnricherConfig::{emit_not_affected, max_components}` |

All public functions return `std::expected<T, Error>` — never throws.

## Examples

### Gate CI on high-or-critical findings

```cpp
auto report = enricher.enrich(doc);
if (!report) return /* transport-failure exit code */;
if (report->has_high_or_critical()) {
    return /* SAST-finding exit code */;
}
```

### Run a bounded smoke check (first 50 components only)

```cpp
EnricherConfig cfg;
cfg.max_components = 50;
Enricher enricher{client, cfg};
```

### Direct PURL query (without an SBOM)

```cpp
auto vulns = OsvClient{}.query_by_purl("pkg:cargo/openssl@0.10.50");
for (const auto& v : *vulns) {
    std::cout << v.id << "  " << severity_name(v.severity) << '\n';
}
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
