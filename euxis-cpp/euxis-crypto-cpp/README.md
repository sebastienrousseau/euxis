# euxis-crypto-cpp

C++23 cryptographic primitives — AES-256-GCM, Ed25519, key derivation via libsodium.

## Overview

euxis-crypto-cpp provides the foundational cryptographic operations used across the Euxis C++ stack. It wraps libsodium to deliver authenticated encryption (AES-256-GCM with additional authenticated data), Ed25519 digital signatures, and Argon2id key derivation. All sensitive memory is securely erased via sodium_memzero on destruction.

## Dependencies

- libsodium
- nlohmann-json

## Building

```bash
# From the euxis-cpp root
cmake -B build -S .
cmake --build build --target euxis-crypto-cpp
```

## Testing

```bash
ctest --test-dir build -R euxis-crypto-cpp_tests
```

## API

- **aes_gcm.hpp** -- AES-256-GCM authenticated encryption and decryption with AAD support.
- **ed25519.hpp** -- Ed25519 key generation, signing, and signature verification.
- **key_derivation.hpp** -- Argon2id key derivation and random key generation.
- **types.hpp** -- Shared type aliases (SecureBytes, KeyPair, Nonce).
- **errors.hpp** -- Exception hierarchy for cryptographic operation failures.
