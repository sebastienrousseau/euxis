# Euxis Library Architecture (`libs/`)

The Euxis C++ runtime is 27 static libraries under `libs/`, each owning one domain. Every library follows the same layout convention (`include/euxis/<name>/` for public headers, `src/` for implementations, `tests/` for GTest sources) and is wired into the build through the `euxis_add_library()` CMake macro.

**Version:** v0.0.2 · **Test files across libs/:** 137 · **Build tree:** `build/cmake-build/`

## Layout convention

Every library directory follows the same shape:

```
libs/<name>/
├── include/euxis/<name>/   Public headers (consumers see euxis/<name>/<header>.hpp)
├── src/                     Implementation files (.cpp)
├── tests/                   GTest sources (test_*.cpp)
├── CMakeLists.txt           Calls euxis_add_library(euxis-<name>-cpp ...)
└── README.md (optional)     Lib-specific contributor notes
```

The macro takes three sections — `SOURCES`, `DEPS`, `TEST_SOURCES` — and produces both the lib target and a `*_tests` executable in one call. Warnings-as-errors, C++23 standard, LTO settings, and GTest linkage are inherited from the root configuration.

## Libraries by domain

### Agentic core (5 libs)

| Lib | What it owns |
|-----|--------------|
| `runtime` | `AgentLoopHarness`, `IterationBudget`, `IContextEngine`, `IToolRegistry`, `IStreamingProvider`, `ISessionStore`, `classify_approval()`, the contracts shim, the reflection schema scaffolding |
| `core` | `Router`, `Supervisor`, `Swarm`, `CredentialPool`, `DelegationCoordinator` — the orchestration primitives |
| `ensemble` | Verifier-quorum runner + real Claude/OpenAI/Gemini HTTP verifiers + deterministic mock verifier |
| `inference` | `IInferenceEngine`, model registry, Anthropic prompt-cache discipline, llama.cpp + Ollama backends |
| `bench` | Custom benchmark runner, reporter, result matrix, plus three opt-in Google Benchmark binaries (`euxis_perf_gbench`, `euxis_scan_gbench`, `euxis_agent_gbench`, `euxis_ensemble_gbench`) |

### Protocol + transport (4 libs)

| Lib | What it owns |
|-----|--------------|
| `a2a` | A2A v0.2 JSON-RPC server, HTTP transport, WebSocket transport, msgpack-encoded agent card + task lifecycle |
| `a2a-types` | Shared wire types — `A2AMessage`, `BidRequest`, `BidResponse`, `TransportError`, `ITransport` |
| `network` | `acp.hpp` (OpenClaw-style control-plane state machine), HTTP/WS transport helpers, resilience primitives (re-exported from `euxis::core::contracts`) |
| `adapters` | Channel adapters — Slack, Discord, Telegram, WhatsApp |

### Verification + analysis (8 libs)

| Lib | What it owns |
|-----|--------------|
| `scan` | OpenGrep-compatible rule-pack ingester + engine v2 (`pattern-not`, `pattern-inside`) |
| `parse` | Tree-sitter parsers for C, C++, Rust, Go, Python, JavaScript, TypeScript, Java |
| `cpg` | Code-property graph: AST projection + intra-procedural CFG + DDG |
| `taint` | Forward data-dependency graph (DDG) taint analyzer |
| `reach` | Reachability analysis for the audit phase + ensemble scaffolding integration |
| `security` | Canonical `Finding` type, SARIF taxa, `Severity`, `Confidence`, `EnsembleVote` |
| `sbom` | CycloneDX 1.6, SPDX 3.0.1, OpenVEX emission |
| `sca` | Cargo.lock and package-lock.json parsers feeding the SBOM stack |
| `slopsquatting` | Typosquat / package-name confusion guard (Spracklen USENIX 2025) |
| `attest` | DSSE signing, in-toto statements, SLSA, Sigstore signed evidence bundles |

### Foundation primitives (7 libs)

| Lib | What it owns |
|-----|--------------|
| `crypto` | AES-GCM (cached key schedule, +33% throughput), BLAKE2b derivation, Ed25519 signing, libsodium primitives |
| `cache` | Libsodium-protected scan cache, content-hashed keys, SQLite-backed storage, `JsonCache` microbench |
| `memory` | Tiered-memory abstractions (hot / warm / cold) |
| `identity` | Identity and credential helpers |
| `bridge` | Inter-process bridge abstractions |
| `platform` | Local + Docker execution backends (`IExecutionBackend`, `LocalBackend`, `DockerBackend`) |
| `metrics` | `UsageRecord`, `ProviderPricing`, `aggregate()`, `SessionInsights` |

### Output (1 lib)

| Lib | What it owns |
|-----|--------------|
| `publisher` | Release packaging — TeX template substitution via inja, PDF emission, signed-artifact bundles |

## Dependency graph

Strictly downward; no cycles. Verified by grepping every `CMakeLists.txt` `DEPS` block in this tree.

```
                                    ┌─────────────┐
                                    │ apps/{cli,  │
                                    │ gateway,    │
                                    │ publisher,  │
                                    │ etx}        │
                                    └──────┬──────┘
                                           │
       ┌───────────────────────────────────┼───────────────────────────────────┐
       ▼                                   ▼                                   ▼
  ┌──────────┐   ┌──────────┐    ┌─────────────────┐               ┌─────────────────┐
  │ runtime  │   │ core     │    │ ensemble        │               │ scan/cpg/taint  │
  │          │◄──┤          │    │  → security     │               │  → parse, sec.  │
  │  → sec.  │   │  → crypto│    └────────┬────────┘               └────────┬────────┘
  └────┬─────┘   │    metrics│             │                                  │
       │         │    network │             ▼                                  ▼
       │         └────────────┘    ┌─────────────────┐               ┌─────────────────┐
       │                           │ slopsquatting   │               │ reach           │
       │                           │  → sbom, sca,   │               │  → cpg, parse,  │
       │                           │    security     │               │    security     │
       │                           └────────┬────────┘               └─────────────────┘
       │                                    │
       ▼                                    ▼
  ┌──────────┐                       ┌──────────┐
  │ inference│                       │ sca      │
  │  →runtime│                       │  → sbom  │
  └──────────┘                       └──────────┘

  Foundation tier (no euxis-internal deps):
  crypto · cache · memory · identity · network · bridge · a2a-types · sbom · platform · adapters
```

The edges that matter most for refactoring: `runtime → security` (the agentic loop assembles `Finding` objects), `inference → runtime` (model engines consume `IContextEngine` / token estimation), `ensemble → security` (votes attach to findings), and `scan/cpg/taint/reach → parse + security` (the static-analysis stack).

## The `euxis_add_library` macro

The macro is the standard wiring. Every lib uses it; consumers should too.

```cmake
euxis_add_library(euxis-runtime-cpp
  SOURCES
    src/agent_loop.cpp
    src/context_engine.cpp
    src/tool_manifest.cpp
    src/tool_registry.cpp
    # … etc
  DEPS
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    yaml-cpp
    euxis-security-cpp
  TEST_SOURCES
    tests/test_agent_loop.cpp
    tests/test_context_engine.cpp
    # … etc
)
```

The macro creates the library target, links GTest into a `<target>_tests` executable, applies warnings-as-errors, registers the install target, and adds the test binary to CTest. Consumers don't have to hand-roll any of that.

## Extension guidelines

### Adding a new library

1. Create `libs/<name>/{include/euxis/<name>/,src/,tests/}` and `libs/<name>/CMakeLists.txt`.
2. Inside the CMakeLists, call `euxis_add_library(euxis-<name>-cpp ...)` with the three sections.
3. Add `add_subdirectory(libs/<name>)` to the root `CMakeLists.txt` in dependency order (libs that depend on yours must come after).
4. Public headers live under `include/euxis/<name>/` so consumers include them as `#include "euxis/<name>/<header>.hpp"`.

### Rules

- **One domain per library.** Don't park HTTP transport in `crypto`; spin up `network` or extend `a2a`.
- **No circular dependencies.** If `runtime` depends on `security`, then `security` must not depend on `runtime` even transitively.
- **Stable public surface.** The headers under `include/euxis/<name>/` are the API contract. Move implementation details into the `src/` tree, not into headers.
- **Tests opt in.** Every lib carries at least one test source; the `_tests` target is created automatically by the macro.
- **Forward-compat hooks live in the consuming lib.** When a feature needs `__cpp_lib_generator` / `__cpp_lib_reflection` / `__cpp_contracts`, the guard goes in the consuming lib's header. See `libs/runtime/include/euxis/runtime/streaming.hpp` for the pattern.

---

*Euxis v0.0.2 · Build something that matters.*
