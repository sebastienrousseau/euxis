# euxis-identity-cpp

C++23 W3C DID, Verifiable Credentials, attestations, identity registry, and ERC-8004 agent cards.

## Overview

euxis-identity-cpp provides decentralized identity primitives for Euxis agents. It supports W3C DID document creation and resolution, HMAC-SHA256 Verifiable Credentials issuance and verification, agent attestations, and a pluggable identity registry (with an InMemoryIdentityRegistry default). It also generates ERC-8004-compliant agent cards for on-chain identity registration.

## Dependencies

- libsodium
- nlohmann-json
- spdlog

## Building

```bash
# From the euxis-cpp root
cmake -B build -S .
cmake --build build --target euxis-identity-cpp
```

## Testing

```bash
ctest --test-dir build -R euxis-identity-cpp_tests
```

## API

- **did.hpp** -- W3C DID document creation, serialization, and resolution.
- **credentials.hpp** -- HMAC-SHA256 Verifiable Credentials issuance and verification.
- **attestation.hpp** -- Agent attestation creation and validation.
- **registry.hpp** -- IdentityRegistry interface and InMemoryIdentityRegistry implementation.
- **erc8004.hpp** -- ERC-8004 agent card generation and JSON serialization.
