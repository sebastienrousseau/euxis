# Package Structure Matrix

Generated from `euxis-ops/quality/package_standards.json`.

| Package | Kind | Path | Required Files | Tests | Docs | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `euxis-sdk` | `rust` | `euxis-sdk` | `Cargo.toml, README.md` (yes) | `euxis-sdk/tests` (yes) | `data/docs/modules/euxis-sdk.md` (yes) | `ok` |
| `euxis-web` | `node` | `euxis-web` | `package.json, README.md` (yes) | `euxis-web/src/crypto-lib/tests` (yes) | `data/docs/modules/euxis-web.md` (yes) | `ok` |
| `euxis-crypto-cpp` | `cpp-library` | `euxis-cpp/euxis-crypto-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-crypto-cpp/tests` (yes) | `data/docs/modules/euxis-crypto-cpp.md` (yes) | `ok` |
| `euxis-bridge-cpp` | `cpp-library` | `euxis-cpp/euxis-bridge-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-bridge-cpp/tests` (yes) | `data/docs/modules/euxis-bridge-cpp.md` (yes) | `ok` |
| `euxis-memory-cpp` | `cpp-library` | `euxis-cpp/euxis-memory-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-memory-cpp/tests` (yes) | `data/docs/modules/euxis-memory-cpp.md` (yes) | `ok` |
| `euxis-identity-cpp` | `cpp-library` | `euxis-cpp/euxis-identity-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-identity-cpp/tests` (yes) | `data/docs/modules/euxis-identity-cpp.md` (yes) | `ok` |
| `euxis-inference-cpp` | `cpp-library` | `euxis-cpp/euxis-inference-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-inference-cpp/tests` (yes) | `data/docs/modules/euxis-inference-cpp.md` (yes) | `ok` |
| `euxis-a2a-cpp` | `cpp-library` | `euxis-cpp/euxis-a2a-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-a2a-cpp/tests` (yes) | `data/docs/modules/euxis-a2a-cpp.md` (yes) | `ok` |
| `euxis-security-cpp` | `cpp-library` | `euxis-cpp/euxis-security-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-security-cpp/tests` (yes) | `data/docs/modules/euxis-security-cpp.md` (yes) | `ok` |
| `euxis-scripts-cpp` | `cpp-library` | `euxis-cpp/euxis-scripts-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-scripts-cpp/tests` (yes) | `data/docs/modules/euxis-scripts-cpp.md` (yes) | `ok` |
| `euxis-runtime-cpp` | `cpp-library` | `euxis-cpp/euxis-runtime-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-runtime-cpp/tests` (yes) | `data/docs/modules/euxis-runtime-cpp.md` (yes) | `ok` |
| `euxis-metrics-cpp` | `cpp-library` | `euxis-cpp/euxis-metrics-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-metrics-cpp/tests` (yes) | `data/docs/modules/euxis-metrics-cpp.md` (yes) | `ok` |
| `euxis-core-cpp` | `cpp-library` | `libs/core` | `CMakeLists.txt` (yes) | `libs/core/tests` (yes) | `data/docs/modules/euxis-core-cpp.md` (yes) | `ok` |
| `euxis-adapters-cpp` | `cpp-library` | `euxis-cpp/euxis-adapters-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-adapters-cpp/tests` (yes) | `data/docs/modules/euxis-adapters-cpp.md` (yes) | `ok` |
| `euxis-gateway-cpp` | `cpp-library` | `euxis-cpp/euxis-gateway-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-gateway-cpp/tests` (yes) | `data/docs/modules/euxis-gateway-cpp.md` (yes) | `ok` |
| `euxis-cli-cpp` | `cpp-application` | `euxis-cpp/euxis-cli-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-cli-cpp/tests` (yes) | `data/docs/modules/euxis-cli-cpp.md` (yes) | `ok` |
| `euxis-bench-cpp` | `cpp-library` | `euxis-cpp/euxis-bench-cpp` | `CMakeLists.txt` (yes) | `euxis-cpp/euxis-bench-cpp/tests` (yes) | `data/docs/modules/euxis-bench-cpp.md` (yes) | `ok` |
| `euxis-etx` | `cpp-application` | `euxis-cpp/euxis-etx` | `CMakeLists.txt` (yes) | `-` (yes) | `data/docs/modules/euxis-etx.md` (yes) | `ok` |

## Enforcement

- Local generate: `make package-structure-matrix`
- Local check: `make package-structure-matrix-check`
- CI: enforced via `make gate-all`.
