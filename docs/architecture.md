# Euxis Architecture

Euxis is a C++23 multi-agent code-certification toolkit. The system is a stack of 27 static libraries under `libs/` feeding four binaries under `apps/`, all driven by CMake 3.28 with FetchContent for third-party dependencies. The same agent fleet (53 agents, 6 squads under `data/agents/`) the original shell prototype defined now executes against the C++ runtime.

**Version:** v0.0.2

## System layers

The system is layered top-down with strict downward dependencies — no upward calls, no cycles. The four apps consume the runtime + verification stacks; the runtime stack consumes the foundation stack; the foundation stack depends only on third-party deps.

```
┌───────────────────────────────────────────────────────────────────────────┐
│                           Application binaries                            │
│   ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────────────┐  │
│   │  euxis     │  │ gateway    │  │ publisher  │  │ etx (Qt6 desktop)  │  │
│   │  (apps/cli)│  │            │  │            │  │                    │  │
│   └────────────┘  └────────────┘  └────────────┘  └────────────────────┘  │
└───────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                       Agentic + verification stack                        │
│   runtime  · core  · ensemble  · inference  · a2a  · adapters  · publisher │
│   metrics  · platform  · reach  · scan  · taint  · cpg  · slopsquatting    │
│   security  · sca  · sbom  · attest  · parse                              │
└───────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                            Foundation stack                               │
│   crypto  · cache  · memory  · identity  · network  · bridge  · a2a-types │
└───────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  Third-party (FetchContent): nlohmann_json · spdlog · yaml-cpp · inja ·   │
│  ixwebsocket · cpp-httplib · GTest · Google Benchmark (opt-in) ·          │
│  SQLite3 · libsodium                                                      │
└───────────────────────────────────────────────────────────────────────────┘
```

## What each binary does

`apps/cli` produces the main `euxis` CLI. It depends on essentially every lib in the tree — its commands cover the full surface, from the agentic loop (`euxis sdk-demo`) to scans, sbom emission, signed attestations, publishing, and the agent dispatch entry points the shell prototype used to host.

`apps/gateway` produces the HTTP gateway. It depends on adapters + core + crypto + gateway + metrics + security. The agent-control plane logic from OpenClaw-style ACP lives in `libs/network/acp.hpp` and the gateway surfaces it over HTTP.

`apps/publisher` produces the release-packager binary. Depends on `libs/publisher` and `libs/runtime` only — small, focused.

`apps/etx` is the Qt6 desktop GUI (one binary, not a Python Textual TUI as earlier versions of this document described). Depends on bridge + crypto + identity + inference + memory. The widget and theme tests live under `apps/etx/tests/`.

## The runtime stack at a glance

`libs/runtime` is the agentic core. It exposes `AgentLoopHarness` (iteration budget + context engine + conversation vector), `IToolRegistry`, `IStreamingProvider` (coroutine-based, guarded by `__cpp_lib_generator`), the `IContextEngine` strategy interface with a default `WindowedContextEngine`, `ApprovalClass` + `classify_approval()` for tool-call gating, and `ISessionStore` for branch-aware conversation persistence.

`libs/core` exposes the swarm orchestration primitives: `Router` (capability-based agent selection), `Supervisor` (state-machine for delegated subagents), `CredentialPool` (thread-safe per-key cooldown + jittered backoff), and `DelegationCoordinator` (parallel fan-out matching Hermes-style subagent semantics).

`libs/ensemble` runs the verifier-quorum loop from CodeX-Verify: real Claude / OpenAI / Gemini HTTP verifiers vote on each finding and the runner applies a quorum rule plus optional unanimity. `libs/inference` carries the lower-level engine interface (`IInferenceEngine`) plus the Anthropic prompt-cache discipline helpers.

`libs/a2a` ships the A2A v0.2 JSON-RPC surface — agent card, task lifecycle, HTTP transport, WebSocket transport, msgpack serialisation. `libs/a2a-types` carries the shared wire types so other libs depend on them without pulling in the transport layer.

## The verification stack at a glance

`libs/scan` is the OpenGrep-compatible rule-pack ingester and engine. `libs/cpg` builds the code-property graph (AST + CFG + DDG triad) on top of `libs/parse` (tree-sitter for C, C++, Rust, Go, Python, JavaScript, TypeScript, Java). `libs/taint` runs forward data-flow propagation on the CPG; `libs/reach` does reachability analysis for the audit phase.

`libs/security` carries the canonical `Finding` type, SARIF taxa, severity / confidence enums, and the `EnsembleVote` shape the ensemble runner attaches. `libs/sbom` emits CycloneDX 1.6, SPDX 3.0.1, and OpenVEX. `libs/sca` parses Cargo.lock and package-lock.json and feeds the SBOM stack. `libs/slopsquatting` is the typosquat guard (Spracklen USENIX 2025). `libs/attest` carries Sigstore-signed in-toto / DSSE / SLSA evidence-bundle emission.

## Foundation stack at a glance

`libs/crypto` is the cryptographic core — AES-GCM with cached key schedules (33% faster throughput), BLAKE2b key derivation, Ed25519 signing, libsodium-backed primitives. `libs/cache` is a libsodium-protected scan-result cache with SQLite-backed storage and content-hash invalidation. `libs/memory` carries the tiered-memory abstractions. `libs/identity`, `libs/network`, `libs/bridge` round out the foundation.

## Agent hierarchy

The agent fleet is data-defined under `data/agents/registry.json` (53 agents) and `data/agents/squads.json` (6 squads + combo definitions). Two tiers — core (strategic) and fleet (tactical) — match the prompt structure under `data/agents/prompts/core/` and `data/agents/prompts/fleet/`.

| Tier | Role | Notes |
|------|------|-------|
| **Core** | Strategic planning, governance, quality assurance | Orchestrator, architect, planner, reviewer, librarian, auditor, critic, arbiter, historian, route, pair, guard |
| **Fleet** | Tactical domain specialists | Debuggers, sentinels, writers, gatekeepers, inspectors, pentesters, polyglots, repairers, responders, etc. |

The runtime resolves an agent by name via `Router::resolve(intent)` from `libs/core`, then `AgentLoopHarness` drives the model turn through `IProvider::execute` (sync) or `IStreamingProvider::execute_stream` (coroutine).

## Capability routing

Capabilities are tags attached to each agent in `registry.json`. `libs/core/router.hpp` matches an incoming intent against the capability tags and returns the best-fit agent. Categories include analysis (code-review, architecture-review, performance-profiling), security (security-audit, compliance-check), development (debugging, documentation), operations (cicd-pipeline, infrastructure-provisioning), and UI/UX (tui-design, accessibility).

## Session lifecycle

```
1. CLI command received (apps/cli)
2. Health check (euxis doctor probes; toolchain + provider creds)
3. Context detection (libs/runtime; project type + scope)
4. Agent resolution (libs/core/router.hpp)
5. Memory retrieval (libs/memory; tiered hot/warm/cold)
6. Prompt assembly (libs/runtime; protocol injection + template substitution)
7. Provider execution (libs/inference for raw, libs/ensemble for quorum)
8. Tool-call loop (libs/runtime/agent_loop.hpp + IToolRegistry)
9. Session persistence (libs/runtime/session_store.hpp)
10. Memory update (libs/memory; promote hot → warm on session close)
```

## Repository layout

```
~/.euxis/
├── apps/                       Production binaries (one per directory)
│   ├── cli/                    Main `euxis` CLI
│   ├── gateway/                HTTP gateway
│   ├── publisher/              Release packager
│   └── etx/                    Qt6 desktop GUI
├── libs/                       27 static libraries (euxis_add_library macro)
│   ├── runtime/                Agentic core
│   ├── ensemble/               Verifier quorum
│   ├── inference/              Provider abstractions
│   ├── a2a/, a2a-types/        A2A v0.2 protocol
│   ├── scan/, cpg/, parse/     Code analysis stack
│   ├── taint/, reach/          Dataflow + reachability
│   ├── security/, sbom/, sca/  Verification + supply-chain
│   ├── slopsquatting/, attest/ Supply-chain + provenance
│   ├── crypto/, cache/         Foundation primitives
│   └── ...                     and 8 more
├── apps/cli/CMakeLists.txt     The wiring is CMake, not shell sourcing
├── CMakeLists.txt              Root: project(euxis VERSION 0.0.2 LANGUAGES CXX)
├── cmake/                      Custom modules (EuxisStdModule, EuxisDocs, ...)
├── build/cmake-build/          Default build tree
├── data/agents/                Registry, squads, prompts
├── docs/                       This tree
└── ...
```

## Forward-compat pilots

Four guard-removal hooks let the codebase adopt C++23/26 features today and pick up native compiler support without churning call sites: `import std;` (opt-in via `EUXIS_ENABLE_STD_MODULE`), C++26 contracts shim (opt-in via `EUXIS_ENABLE_CXX26_CONTRACTS`, runtime-check mode), `std::generator` streaming (`__cpp_lib_generator` guard, GCC 14 has it; libc++ does not yet), and `^^` / `std::meta` reflection for `ToolDeclaration` schemas (`__cpp_lib_reflection` guard, no shipping compiler defines it as of 2026-06). See `libs/runtime/include/euxis/runtime/streaming.hpp`, `contracts.hpp`, `reflect_schema.hpp`, and `cmake/EuxisStdModule.cmake`.

## Extension points

### Adding a new library

Create `libs/<name>/{include/euxis/<name>/,src/,tests/}` and `libs/<name>/CMakeLists.txt` calling `euxis_add_library(euxis-<name>-cpp ...)` with `SOURCES`, `DEPS`, and `TEST_SOURCES` sections. Add `add_subdirectory(libs/<name>)` to the root `CMakeLists.txt` in dependency order. The macro takes care of test binary creation, GTest linking, warnings-as-errors, and the install target.

### Adding a new tool to the agent loop

Define an arg struct, specialise `derive_input_schema<T>` (one-time today; deletes when reflection lands), and register the tool against an `IToolRegistry` with a handler that returns `std::expected<nlohmann::json, std::string>`. See `docs/examples/cpp/tool_calling_loop/main.cpp` for the end-to-end pattern.

### Adding a new provider

Implement `IProvider` (sync) or `IStreamingProvider` (coroutine, guarded). Real provider implementations live under `libs/ensemble/src/providers/` today — Claude, OpenAI, Gemini all use httplib + nlohmann_json for the HTTP request body.

### Adding a new agent

Add the agent prompt under `data/agents/prompts/<tier>/<name>.txt`, register it in `data/agents/registry.json` with its capability tags, and the router picks it up automatically. No code change required.

### Adding a new capability

Add the capability identifier to `data/agents/registry.json` on the agents that should handle it. The router treats capabilities as opaque tags — there's no enum to extend.

---

*Euxis v0.0.2 · Build something that matters.*
