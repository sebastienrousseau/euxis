<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::attest</h1>

<p align="center">
  Cryptographic provenance for euxis scan results: DSSE envelopes,
  in-toto statements, SLSA build provenance, and Sigstore-style bundles
  (cert + Rekor log entries + signatures).
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — sign a scan result, package it as a Sigstore bundle
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/attest)
target_link_libraries(my_app PRIVATE euxis-attest-cpp)
```

## Quick start

```cpp
#include <iostream>
#include <vector>

#include "euxis/attest/dsse.hpp"
#include "euxis/attest/statement.hpp"

int main() {
    using namespace euxis::attest;

    Statement stmt;
    stmt.predicate_type = "https://euxis.co/scan-result/v1";
    stmt.subject.push_back(Subject{
        .name   = "src/",
        .digest = {{"sha256", "abc123..."}},
    });
    stmt.predicate = nlohmann::json{
        {"findings", 0},
        {"scanned_at", "2026-06-18T08:00:00Z"},
    };

    // Sign the statement as a DSSE envelope (signing key from libs/crypto).
    Envelope env;
    env.payload      = stmt.to_canonical_json();
    env.payload_type = "application/vnd.in-toto+json";
    env.signatures.push_back(Signature{
        .keyid = "ed25519:scanner-1",
        .sig   = /* sign env.payload with libs/crypto */ {},
    });
    std::cout << "envelope ready, " << env.signatures.size() << " sig(s)\n";
}
```

## Public surface

| Header | What it owns |
|---|---|
| `statement.hpp` | `Statement`, `Subject` — the in-toto v1 statement shape |
| `dsse.hpp` | `Envelope`, `Signature` — DSSE wrapper around a signed payload |
| `slsa.hpp` | `SlsaProvenance`, `Builder`, `BuildDefinition`, `RunDetails`, `RunMetadata`, `ResolvedDependency` |
| `bundle.hpp` | `Bundle`, `PublicKeyMaterial`, `TlogEntry` — Sigstore-style bundle (cert chain + Rekor entries) |
| `signing.hpp` | `SigningError` + signing-side helpers |

This module emits value types only; the actual signing key material is owned by `libs/crypto`.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
