# euxis-cpp

C++23 core for the Euxis agentic platform, with Qt6 GUI.

## Modules

| Module | Description |
|---|---|
| `euxis-crypto-cpp` | AES-256-GCM, Ed25519, Argon2id key derivation via libsodium |
| `euxis-bridge-cpp` | Skill import, static analysis, admission pipeline, sandbox execution |
| `euxis-memory-cpp` | Tier-bound encrypted memory with AAD isolation |
| `euxis-identity-cpp` | W3C DID, Verifiable Credentials, ERC-8004 agent cards |
| `euxis-inference-cpp` | llama.cpp + Ollama inference, model registry, quality gate |
| `euxis-a2a-cpp` | A2A v0.2 protocol, JSON-RPC server, HTTP transport |
| `euxis-bench-cpp` | Security, autonomy, performance, portability, interop benchmarks |
| `euxis-etx` | Qt6 desktop GUI — 9 screens, 3 themes, vim keybindings |

## Prerequisites

- CMake 3.28+
- C++23 compiler (GCC 13+ or Clang 17+)
- [vcpkg](https://vcpkg.io/) for dependency management
- Qt6 (optional, for euxis-etx GUI)

## Building

```bash
# From the project root
make cpp-build

# Or manually
cmake -B euxis-cpp/build -S euxis-cpp \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build euxis-cpp/build --parallel
```

## Testing

```bash
make cpp-test

# Or manually
ctest --test-dir euxis-cpp/build --output-on-failure
```

## Benchmarks

```bash
make cpp-bench
```

## Dependencies (vcpkg)

- libsodium — cryptographic primitives
- nlohmann-json — JSON serialization
- spdlog / fmt — structured logging
- gtest — Google Test framework
- cpp-httplib — HTTP client/server
- toml11 — TOML configuration
- Qt6 (system) — GUI toolkit

## Code Quality

- ASan + UBSan enabled by default
- clang-tidy with modernize + cppcoreguidelines checks
- clang-format (Google style, C++23)
- `-Wall -Wextra -Wpedantic -Werror`
