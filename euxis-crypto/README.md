# Euxis Cryptography Engine

Secure your Euxis Agent Framework with military-grade, Rust-backed cryptographic primitives.

Use this library to encrypt payloads, strictly manage identities, and guarantee data integrity. We bypassed native Python limitations entirely. This library executes directly via `PyO3` bindings for maximum throughput and zero-copy semantics.

## Core Capabilities

* **Authenticated Encryption:** Encrypt memory using `AES-256-GCM` (Advanced Encryption Standard Galois/Counter Mode). Block tampering instantly.
* **Deterministic Key Derivation:** Generate keys using `PBKDF2-HMAC-SHA256` (Password-Based Key Derivation Function 2).
* **Zero-Copy Architecture:** Pass memory safely. Prevent redundant allocations between Python and Rust.

## Installation

Install the compiled bindings into your environment.

```bash
pip install euxis-crypto
```

## Documentation Architecture

Explore the detailed implementation guides in our `docs/` directory.

* [Overview](docs/index.md): System architecture and threat models.
* [Encryption Standards](docs/encryption.md): Implementation details for `AES-256-GCM`.
* [Key Management](docs/key-derivation.md): Hardening agents via `PBKDF2`.

## Licensing

MIT License - see [LICENSE](LICENSE).

---
*Part of the [Euxis Agent Framework](https://euxis.co)*
