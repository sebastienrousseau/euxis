# Euxis Memory C++

The `euxis::memory` module implements tier-bound, encrypted memory storage for the Agent OS. It ensures that semantic traces and context blocks are cryptographically isolated according to the NHI (Non-Human Identity) IAM matrix.

## Tier-Bound Encryption

The `Store` interface enforces Authenticated Encryption with Associated Data (AEAD).

* **Precondition**: The agent must possess the correct cryptographic credentials for the target memory tier.
* **Postcondition**: Returns a monadic `std::expected` resolving to the decrypted memory entry.

For multi-tenant or multi-agent isolation, use the AES-256-GCM cipher. The system binds the memory entry's AAD (Additional Authenticated Data) to the specific agent ID and tier, preventing unauthorized horizontal access.

## Cache Locality & Zero-Copy Access

The `MemorySessionStore` maps high-frequency memory entries directly into contiguous RAM.

* **SoA**: Structure of Arrays — Hardware-friendly data layout.
* **UB**: Undefined Behavior — Avoid concurrent mutations without explicit synchronization.

For performance-critical recall, the module avoids pointer-chasing `std::map` implementations in favor of flat `std::vector` structures. 

## Semantic Deletion

When an agent requests memory compaction or deletion, the system must definitively destroy the key material.

```cpp
auto res = store.destroy(entry_id)
    .and_then([]() { return verify_wipe(); })
    .or_else([](auto&& err) { return escalate_to_auditor(err); });
```

The system employs C++23 monadic operations to guarantee that destruction errors are mathematically forced into the execution pipeline, preventing silent data persistence.