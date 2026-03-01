# Package Structure Matrix

Generated from `euxis-ops/quality/package_standards.json`.

| Package | Kind | Path | Required Files | Tests | Docs | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `euxis-core` | `python` | `euxis-core` | `pyproject.toml, README.md` (yes) | `euxis-core/tests` (yes) | `euxis-docs/docs/modules/euxis-core.md` (yes) | `ok` |
| `euxis-cli` | `python` | `euxis-cli` | `pyproject.toml, README.md` (yes) | `euxis-cli/tests` (yes) | `euxis-docs/docs/modules/euxis-cli.md` (yes) | `ok` |
| `euxis-gateway` | `python` | `euxis-gateway` | `pyproject.toml, README.md` (yes) | `euxis-gateway/tests` (yes) | `euxis-docs/docs/modules/euxis-gateway.md` (yes) | `ok` |
| `euxis-metrics` | `python` | `euxis-metrics` | `pyproject.toml, README.md` (yes) | `euxis-metrics/tests` (yes) | `euxis-docs/docs/modules/euxis-metrics.md` (yes) | `ok` |
| `euxis-adapters` | `python` | `euxis-adapters` | `pyproject.toml, README.md` (yes) | `euxis-adapters/tests` (yes) | `euxis-docs/docs/modules/euxis-adapters.md` (yes) | `ok` |
| `euxis-security` | `python` | `euxis-security` | `pyproject.toml, README.md` (yes) | `euxis-security/tests` (yes) | `euxis-docs/docs/modules/euxis-security.md` (yes) | `ok` |
| `euxis-runtime` | `runtime-data` | `euxis-runtime` | `README.md, data/perf/metrics.jsonl` (yes) | `-` (yes) | `euxis-docs/docs/modules/euxis-runtime.md` (yes) | `ok` |
| `euxis-scripts` | `ops` | `euxis-scripts` | `README.md` (yes) | `-` (yes) | `euxis-docs/docs/modules/euxis-scripts.md` (yes) | `ok` |
| `euxis-sdk` | `rust` | `euxis-sdk` | `Cargo.toml, README.md` (yes) | `euxis-sdk/tests` (yes) | `euxis-docs/docs/modules/euxis-sdk.md` (yes) | `ok` |
| `euxis-web` | `node` | `euxis-web` | `package.json, README.md` (yes) | `euxis-web/src/crypto-lib/tests` (yes) | `euxis-docs/docs/modules/euxis-web.md` (yes) | `ok` |
| `euxis-docs` | `docs` | `euxis-docs` | `README.md, pyproject.toml` (yes) | `euxis-docs/tests` (yes) | `euxis-docs/docs/modules/euxis-docs.md` (yes) | `ok` |

| `euxis-crypto-cpp` | `cpp-library` | `euxis-cpp/euxis-crypto-cpp` | `CMakeLists.txt, README.md` (yes) | `euxis-cpp/euxis-crypto-cpp/tests` (yes) | `euxis-docs/docs/modules/euxis-crypto-cpp.md` (yes) | `ok` |
| `euxis-bridge-cpp` | `cpp-library` | `euxis-cpp/euxis-bridge-cpp` | `CMakeLists.txt, README.md` (yes) | `euxis-cpp/euxis-bridge-cpp/tests` (yes) | `euxis-docs/docs/modules/euxis-bridge-cpp.md` (yes) | `ok` |
| `euxis-memory-cpp` | `cpp-library` | `euxis-cpp/euxis-memory-cpp` | `CMakeLists.txt, README.md` (yes) | `euxis-cpp/euxis-memory-cpp/tests` (yes) | `euxis-docs/docs/modules/euxis-memory-cpp.md` (yes) | `ok` |
| `euxis-identity-cpp` | `cpp-library` | `euxis-cpp/euxis-identity-cpp` | `CMakeLists.txt, README.md` (yes) | `euxis-cpp/euxis-identity-cpp/tests` (yes) | `euxis-docs/docs/modules/euxis-identity-cpp.md` (yes) | `ok` |
| `euxis-inference-cpp` | `cpp-library` | `euxis-cpp/euxis-inference-cpp` | `CMakeLists.txt, README.md` (yes) | `euxis-cpp/euxis-inference-cpp/tests` (yes) | `euxis-docs/docs/modules/euxis-inference-cpp.md` (yes) | `ok` |
| `euxis-a2a-cpp` | `cpp-library` | `euxis-cpp/euxis-a2a-cpp` | `CMakeLists.txt, README.md` (yes) | `euxis-cpp/euxis-a2a-cpp/tests` (yes) | `euxis-docs/docs/modules/euxis-a2a-cpp.md` (yes) | `ok` |
| `euxis-bench-cpp` | `cpp-library` | `euxis-cpp/euxis-bench-cpp` | `CMakeLists.txt, README.md` (yes) | `euxis-cpp/euxis-bench-cpp/tests` (yes) | `euxis-docs/docs/modules/euxis-bench-cpp.md` (yes) | `ok` |
| `euxis-etx` | `cpp-application` | `euxis-cpp/euxis-etx` | `CMakeLists.txt, README.md` (yes) | `-` (yes) | `euxis-docs/docs/modules/euxis-etx.md` (yes) | `ok` |

## Enforcement

- Local generate: `make package-structure-matrix`
- Local check: `make package-structure-matrix-check`
- CI: enforced via `make gate-all`.
