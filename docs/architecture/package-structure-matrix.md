# Package Structure Matrix

Generated from `euxis-ops/quality/package_standards.json`.

| Package | Kind | Path | Required Files | Tests | Docs | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `euxis-sdk` | `rust` | `euxis-sdk` | `Cargo.toml, README.md` (yes) | `euxis-sdk/tests` (yes) | `docs/modules/euxis-sdk.md` (yes) | `ok` |
| `euxis-web` | `node` | `euxis-web` | `package.json, README.md` (yes) | `euxis-web/src/crypto-lib/tests` (yes) | `docs/modules/euxis-web.md` (yes) | `ok` |
| `euxis-crypto-cpp` | `cpp-library` | `libs/crypto` | `CMakeLists.txt` (yes) | `libs/crypto/tests` (yes) | `docs/modules/euxis-crypto-cpp.md` (yes) | `ok` |
| `euxis-bridge-cpp` | `cpp-library` | `libs/bridge` | `CMakeLists.txt` (yes) | `libs/bridge/tests` (yes) | `docs/modules/euxis-bridge-cpp.md` (yes) | `ok` |
| `euxis-memory-cpp` | `cpp-library` | `libs/memory` | `CMakeLists.txt` (yes) | `libs/memory/tests` (yes) | `docs/modules/euxis-memory-cpp.md` (yes) | `ok` |
| `euxis-identity-cpp` | `cpp-library` | `libs/identity` | `CMakeLists.txt` (yes) | `libs/identity/tests` (yes) | `docs/modules/euxis-identity-cpp.md` (yes) | `ok` |
| `euxis-inference-cpp` | `cpp-library` | `libs/inference` | `CMakeLists.txt` (yes) | `libs/inference/tests` (yes) | `docs/modules/euxis-inference-cpp.md` (yes) | `ok` |
| `euxis-a2a-cpp` | `cpp-library` | `libs/a2a` | `CMakeLists.txt` (yes) | `libs/a2a/tests` (yes) | `docs/modules/euxis-a2a-cpp.md` (yes) | `ok` |
| `euxis-security-cpp` | `cpp-library` | `libs/security` | `CMakeLists.txt` (yes) | `libs/security/tests` (yes) | `docs/modules/euxis-security-cpp.md` (yes) | `ok` |
| `euxis-scripts-cpp` | `cpp-library` | `scripts/scripts-cpp` | `CMakeLists.txt` (yes) | `scripts/scripts-cpp/tests` (yes) | `docs/modules/euxis-scripts-cpp.md` (yes) | `ok` |
| `euxis-runtime-cpp` | `cpp-library` | `libs/runtime` | `CMakeLists.txt` (yes) | `libs/runtime/tests` (yes) | `docs/modules/euxis-runtime-cpp.md` (yes) | `ok` |
| `euxis-metrics-cpp` | `cpp-library` | `libs/metrics` | `CMakeLists.txt` (yes) | `libs/metrics/tests` (yes) | `docs/modules/euxis-metrics-cpp.md` (yes) | `ok` |
| `euxis-core-cpp` | `cpp-library` | `libs/core` | `CMakeLists.txt` (yes) | `libs/core/tests` (yes) | `docs/modules/euxis-core-cpp.md` (yes) | `ok` |
| `euxis-adapters-cpp` | `cpp-library` | `libs/adapters` | `CMakeLists.txt` (yes) | `libs/adapters/tests` (yes) | `docs/modules/euxis-adapters-cpp.md` (yes) | `ok` |
| `euxis-gateway-cpp` | `cpp-application` | `apps/gateway` | `CMakeLists.txt` (yes) | `apps/gateway/tests` (yes) | `docs/modules/euxis-gateway-cpp.md` (yes) | `ok` |
| `euxis-cli-cpp` | `cpp-application` | `apps/cli` | `CMakeLists.txt` (yes) | `apps/cli/tests` (yes) | `docs/modules/euxis-cli-cpp.md` (yes) | `ok` |
| `euxis-bench-cpp` | `cpp-library` | `libs/bench` | `CMakeLists.txt` (yes) | `libs/bench/tests` (yes) | `docs/modules/euxis-bench-cpp.md` (yes) | `ok` |
| `euxis-etx` | `cpp-application` | `apps/etx` | `CMakeLists.txt` (yes) | `apps/etx/tests` (yes) | `docs/modules/euxis-etx.md` (yes) | `ok` |

## Enforcement

- Local generate: `make package-structure-matrix`
- Local check: `make package-structure-matrix-check`
- CI: enforced via `make gate-all`.
