# Euxis Cryptography: Overview

Welcome to the Euxis Cryptography Engine documentation.

Use this documentation to integrate high-performance cryptographic primitives into your agent workflows. We designed this library to provide absolute memory safety and native execution speeds.

## Architectural Foundation

We architected this library specifically for the Euxis Agent Framework. The Python ecosystem often suffers from performance bottlenecks during heavy cryptographic workloads. To solve this, we implemented the core logic entirely in Rust and exposed it via `PyO3` bindings. 

This approach guarantees:
* **Memory Safety:** Rust prevents buffer overflows and dangling pointers during cipher execution.
* **Constant-Time Verification:** The library resists timing attacks during MAC validation.
* **Zero-Copy Serialization:** Python passes byte arrays directly to the native Rust engine.

## Threat Model

We engineered this library to mitigate specific attack vectors:

1. **Tampering:** `AES-256-GCM` inherently rejects altered ciphertexts.
2. **Rainbow Tables:** `PBKDF2-HMAC-SHA256` mandates cryptographic salt to neutralize pre-computed password hashes.
3. **Replay Attacks:** Ensure you generate secure, randomized initialization vectors (nonces) for every encryption call.

## Next Steps

Review the specific component implementations:

* [Encryption Standards](encryption.md)
* [Key Management](key-derivation.md)
