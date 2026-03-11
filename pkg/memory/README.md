# euxis-memory-cpp

C++23 tier-bound encrypted memory store with AAD-based isolation.

## Overview

euxis-memory-cpp implements a tier-bound encrypted memory system for Euxis agents. Memory entries are classified into Hot, Relevant, and CrossAgent tiers, each encrypted with AES-256-GCM using tier-specific additional authenticated data (AAD) to prevent cross-tier decryption. Per-agent keys are derived via Argon2id, and all sensitive material is securely erased with sodium_memzero. Storage uses append-only JSONL files.

## Dependencies

- euxis-crypto-cpp
- nlohmann-json
- spdlog

## Building

```bash
# From the euxis-cpp root
cmake -B build -S .
cmake --build build --target euxis-memory-cpp
```

## Testing

```bash
ctest --test-dir build -R euxis-memory-cpp_tests
```

## API

- **store.hpp** -- EncryptedMemoryStore: put, get, list, and delete operations with tier enforcement.
- **entry.hpp** -- MemoryEntry data model (id, tier, ciphertext, nonce, timestamp, metadata).
- **tier.hpp** -- Tier enum (Hot, Relevant, CrossAgent) and AAD derivation logic.
