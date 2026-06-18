<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::identity</h1>

<p align="center">
  Agent identity primitives for euxis: W3C DID documents, Verifiable
  Credentials, ERC-8004 agent cards, attestations, and an identity registry.
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
add_subdirectory(libs/identity)
target_link_libraries(my_app PRIVATE euxis-identity-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/identity/did.hpp"
#include "euxis/identity/registry.hpp"

int main() {
    using namespace euxis::identity;

    DIDDocument doc;
    doc.id = "did:euxis:scan-agent-001";
    doc.verification_methods.push_back(VerificationMethod{
        .id            = doc.id + "#keys-1",
        .type          = "Ed25519VerificationKey2020",
        .controller    = doc.id,
        .public_key_pem = "-----BEGIN PUBLIC KEY-----\n...",
    });

    InMemoryIdentityRegistry registry;
    AgentIdentity agent{doc.id, std::move(doc)};
    registry.add(std::move(agent));

    const auto found = registry.lookup("did:euxis:scan-agent-001");
    std::cout << "agent registered: " << (found != nullptr) << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `did.hpp` | `DIDDocument`, `VerificationMethod`, `ServiceEndpoint` (W3C DID spec) |
| `credentials.hpp` | `VerifiableCredential`, `Claim`, `Proof` |
| `erc8004.hpp` | `ERC8004AgentCard` — ERC-8004 metadata for autonomous agents |
| `attestation.hpp` | `Attestation` — signed third-party assertion |
| `registry.hpp` | `IdentityRegistry` (abstract), `InMemoryIdentityRegistry`, `AgentIdentity` |

## Examples

### Issue and verify a `VerifiableCredential`

```cpp
#include "euxis/identity/credentials.hpp"

using namespace euxis::identity;

VerifiableCredential vc;
vc.issuer  = "did:euxis:root";
vc.subject = "did:euxis:scan-agent-001";
vc.claims.push_back(Claim{.key = "capability", .value = "scan"});
// Sign + verify against the issuer's DID document via the application's
// own signing path (libs/crypto Ed25519).
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
