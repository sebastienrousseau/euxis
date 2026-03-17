# Euxis Crypto C++

The `euxis::crypto` module enforces the cryptographic backbone of the Agent OS. It provides secure primitives for identity verification, message signing, and zero-knowledge memory encryption.

## Native Cryptographic Primitives

The module is a strict, memory-safe wrapper around `libsodium`.

* **Precondition**: The `libsodium` library must be correctly provisioned by the build system.
* **Postcondition**: Exposes deterministic, side-channel resistant operations.

For symmetric authenticated encryption, use AES-256-GCM. For asymmetric signatures, use Ed25519. Key derivation employs Argon2id for maximum resistance against GPU-based brute forcing.

## Ed25519 Signature Enforcement

Every command mutating the Agent OS state must be cryptographically signed.

* **SFINAE**: Substitution Failure Is Not An Error — Template resolution mechanism.

Dispatch signature verification requests to the `verify_signature` method.

```cpp
auto valid = verify_signature(payload, signature, public_key);
if (!valid) {
    // Escalate to sentinel-identity agent
}
```

## Memory Safety

Never construct cryptographic buffers using standard `std::string` allocations without explicit memory wiping upon destruction.

* **RAII**: Resource-bound lifetime management — Ensure sensitive key material is zeroes out when the scope ends.
* **std::span**: Bounds-checked memory view — Safe non-owning array views.

Use `std::span` when passing binary buffers into encryption routines to prevent out-of-bounds reads and writes.