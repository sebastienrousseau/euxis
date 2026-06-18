<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::crypto</h1>

<p align="center">
  Cryptographic primitives for euxis: AES-256-GCM (simple + cached schedule),
  Ed25519 signatures, BLAKE2b key derivation. libsodium-backed, C++23.
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
- [Performance](#performance)
- [Testing](#testing)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/crypto)
target_link_libraries(my_app PRIVATE euxis-crypto-cpp)
```

The library is a thin, memory-safe wrapper around `libsodium`. Provision libsodium via `pkg-config` (`libsodium-dev` on Debian/Ubuntu, `brew install libsodium` on macOS) or the project's CMake will find it automatically.

## Quick start

```cpp
#include <iostream>
#include <vector>

#include "euxis/crypto/aes_gcm.hpp"
#include "euxis/crypto/ed25519.hpp"

int main() {
    using namespace euxis::crypto;

    // ---- AES-256-GCM encrypt/decrypt ----
    auto key_vec = generate_key(32);
    std::array<std::byte, 32> key{};
    std::copy_n(key_vec.begin(), 32, key.begin());

    const std::vector<std::byte> plaintext(64, std::byte{'X'});

    auto enc = encrypt(plaintext, key);                 // -> std::expected<Ciphertext, Error>
    if (!enc) { std::cerr << "encrypt failed\n"; return 1; }

    auto dec = decrypt(enc->ciphertext, key, enc->iv);
    if (!dec) { std::cerr << "decrypt failed\n"; return 1; }
    std::cout << "round-trip: " << dec->size() << " bytes\n";

    // ---- Ed25519 sign + verify ----
    auto keys = ed25519::generate_keypair();
    const std::string message = "audit me";
    const auto signature = ed25519::sign(message, keys.private_key);
    const bool ok = ed25519::verify(message, signature, keys.public_key);
    std::cout << "signature valid: " << std::boolalpha << ok << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `aes_gcm.hpp` | `encrypt`/`decrypt` (simple) + `AesGcmContext::create` (cached key schedule, +33% throughput) |
| `ed25519.hpp` | `generate_keypair`, `sign`, `verify` |
| `key_derivation.hpp` | `derive_key(password, salt, iterations, length)` — BLAKE2b backend with a `iterations == 0` fast path |
| `errors.hpp` | `Error` enum + `error_message(Error)` |
| `types.hpp` | `Ciphertext`, key/iv aliases, `generate_key(n)` |

All functions return `std::expected<T, Error>` rather than throwing. The `Error` enum is exhaustive and stable; treat it as the error contract.

## Examples

### Cached AES-GCM context (hot path)

```cpp
auto ctx = AesGcmContext::create(key);              // expand the key schedule once
if (!ctx) { /* handle */ }

for (auto& chunk : message_stream) {
    auto enc = ctx->encrypt(chunk);
    transmit(enc->ciphertext, enc->iv);
}
```

Use the cached context whenever you encrypt more than one buffer with the same key — it amortises the AES key-schedule expansion across calls (~33% speedup on long streams in the project's gbench).

### BLAKE2b key derivation

```cpp
#include "euxis/crypto/key_derivation.hpp"

auto dk = derive_key(password_bytes,
                     salt_bytes,
                     /*iterations=*/0,              // 0 = fast path (single BLAKE2b)
                     /*key_length=*/32);
```

Set `iterations > 0` to enable iterated key stretching when the input is a low-entropy passphrase. For high-entropy random material (already-32-bytes), `iterations=0` is the right choice and is significantly faster.

## Performance

Measured on Apple M1 with `EUXIS_BUILD_GBENCH=ON`:

| Bench | Throughput |
|---|---|
| `BM_CryptoThroughput_Simple` | ~310 MiB/s on a 1 KiB payload |
| `BM_CryptoThroughput_Cached` | ~410 MiB/s on a 1 KiB payload (+33%) |
| `BM_KeyDerivation_FastPath` (BLAKE2b, iterations=0) | <2 µs |

Reproduce with `cmake --build build/cmake-build --target euxis_perf_gbench`.

## Testing

```bash
cmake --build build/cmake-build --target euxis-crypto-cpp_tests
./build/cmake-build/libs/crypto/euxis-crypto-cpp_tests
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
