<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::runtime</h1>

<p align="center">
  The agentic core: <code>AgentLoopHarness</code>, iteration budget,
  pluggable context-compaction, tool registry, approval classifier,
  coroutine-based streaming, and session persistence. C++23, header-first.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

**Getting started**
- [Install](#install) — CMake target wiring
- [Quick start](#quick-start) — one closed agent turn in twenty lines

**Library reference**
- [Overview](#overview) — what this module owns
- [Public surface](#public-surface) — header-by-header map
- [Examples](#examples) — driving the loop, gating tools, streaming
- [Forward-compat pilots](#forward-compat-pilots) — guard-removal hooks

**Operational**
- [Testing](#testing)
- [License](#license)

---

## Install

This module is part of the euxis monorepo. Consume it from the same CMake build:

```cmake
add_subdirectory(libs/runtime)
target_link_libraries(my_app PRIVATE euxis-runtime-cpp)
```

The static library exposes its public headers under `include/euxis/runtime/`. Include them as `#include "euxis/runtime/<header>.hpp"`.

## Quick start

```cpp
#include <iostream>
#include <string>

#include "euxis/runtime/agent_loop.hpp"
#include "euxis/runtime/agent_session.hpp"

int main() {
    using namespace euxis::runtime;

    // Build one harness — owns the iteration budget, the context-compaction
    // engine, and the conversation vector.
    AgentLoopHarness::Config cfg;
    cfg.session_id           = "demo-001";
    cfg.agent_id             = "demo-agent";
    cfg.iteration_budget_max = 5;
    cfg.context_window       = 16'384;
    AgentLoopHarness harness{std::move(cfg)};

    // Seed the transcript.
    SessionMessage user{};
    user.role     = Role::User;
    user.content  = "Audit the auth module.";
    user.agent_id = "demo-agent";
    harness.add_message(ConversationMessage{user});

    // Drive one turn. The closure is your provider call — wire it to
    // Anthropic / OpenAI / Ollama / a mock as appropriate.
    auto turn = harness.run_turn([] {
        ProviderResponse r{};
        r.success = true;
        r.output  = R"({"final":"audit complete: no findings"})";
        return r;
    });

    std::cout << "budget consumed: " << turn.budget_consumed << '\n'
              << "messages: "        << turn.messages_after << '\n'
              << "output: "          << turn.provider_response.output << '\n';
}
```

## Overview

`euxis::runtime` is the agentic substrate every Euxis app links against. It owns the loop primitives but stays **provider-agnostic** and **tool-agnostic** — the harness does not assume a JSON tool-call dialect or a specific LLM vendor. Callers wire those in.

In scope:
- One-turn execution harness with bounded iterations, context-compaction queries, and conversation persistence.
- Tool registry and the `ApprovalClass` gating taxonomy.
- Coroutine-based streaming provider interface (`std::generator<ProviderDelta>`, guarded).
- Session store interfaces (in-memory and msgpack-on-disk implementations).
- C++26 contracts shim with a four-way fallback.
- Compile-time JSON-Schema derivation for tool argument structs.

Deliberately out of scope: real HTTP transport (lives in `libs/inference`), verifier-ensemble logic (lives in `libs/ensemble`), code-property-graph or scan logic (lives in `libs/cpg`, `libs/scan`).

## Public surface

| Header | What it owns |
|---|---|
| `agent_loop.hpp` | `AgentLoopHarness` — bundles iteration budget + context engine + conversation vector into one type that drives a single agent turn. |
| `iteration_budget.hpp` | `IterationBudget` — atomic CAS counter, defaults 90 (parent) / 50 (subagent). |
| `context_engine.hpp` | `IContextEngine` + `WindowedContextEngine` — pure compaction-plan strategy. Head/tail preservation; caller pays the summary model cost. |
| `tool.hpp` | `ITool` — minimal "external tool" interface (`declaration()` + `execute(json)`). |
| `agent_session.hpp` | `IToolRegistry`, `ToolDeclaration`, `ToolHandler`, `ConversationMessage`, `AgentSession`. |
| `tool_manifest.hpp` | `ToolDeclaration_v2`, `ToolManifest`, `ApprovalClass`, `classify_approval()` — ACP-style approval taxonomy. |
| `provider.hpp` | `IProvider` (sync), `ProviderResponse`, `ModelSpec`. |
| `streaming.hpp` | `IStreamingProvider` (coroutine, guarded by `EUXIS_HAS_STD_GENERATOR`), `ProviderDelta`, `DeltaKind`. |
| `session_store.hpp` | `ISessionStore`, `SessionMessage`, `SessionSnapshot`, factories for filesystem + memory stores. |
| `reflect_schema.hpp` | `SchemaBuilder`, `json_type_for<T>()`, `derive_input_schema<T>()` with a `__cpp_lib_reflection` forward hook. |
| `contracts.hpp` | `EUXIS_PRE` / `EUXIS_POST` / `EUXIS_CONTRACT_ASSERT` — four-way C++26 contracts shim. |
| `validator.hpp` | Snapshot validation helpers. |
| `lifecycle.hpp`, `interaction.hpp`, `manifesto.hpp`, `platform.hpp`, `config.hpp` | Runtime configuration + lifecycle hooks. |

## Examples

### Approval-gated tool dispatch

```cpp
#include "euxis/runtime/tool_manifest.hpp"

using namespace euxis::runtime;

// Pure heuristic; safe to call on the hot path.
ToolDeclaration_v2 decl;
decl.name = "scan";
const auto klass = classify_approval(decl);          // ApprovalClass::Readonly

decl.name = "write_file";
const auto klass2 = classify_approval(decl);         // ApprovalClass::ExecCapable

decl.name = "list_sessions";
const auto klass3 = classify_approval(decl, "admin:sessions"); // ControlPlane
```

### Pluggable context compaction

```cpp
#include "euxis/runtime/context_engine.hpp"
#include <vector>

using namespace euxis::runtime;

std::vector<ConversationMessage> msgs = /* ... */;

WindowedContextEngine engine;                       // Hermes-tuned defaults
const auto tokens = estimate_tokens(msgs);          // 4-bytes-per-token heuristic
const auto plan   = engine.plan(msgs, tokens, /*context_window=*/200'000);

if (!plan.is_empty()) {
    // Caller summarises [plan.compact_start, plan.compact_end) using a
    // cheap auxiliary model. The engine does not call the model itself.
}
```

### Reading a session lazily (when the toolchain has `<generator>`)

```cpp
auto store = make_memory_session_store();
// ...load / save against `store`...

for (auto&& msg : store->stream_episodes("session-1")) {
    process(msg);                                    // co_yield one at a time
}
```

On toolchains without `<generator>`, the same call returns `std::vector<SessionMessage>` and the range-for is identical.

## Forward-compat pilots

This module ships three guard-removal hooks. Each adopts the C++23/26 semantics today and picks up native compiler support the day a toolchain ships it, without changing any call site.

- **`EUXIS_HAS_STD_GENERATOR`** — defined in `streaming.hpp` when `__cpp_lib_generator >= 202207L`. Gates `IStreamingProvider` and the lazy `stream_episodes` path.
- **`EUXIS_CONTRACTS_RUNTIME`** — opt-in via top-level CMake option `EUXIS_ENABLE_CXX26_CONTRACTS=ON`. Turns the contracts shim into death-test-able runtime checks.
- **`__cpp_lib_reflection`** — auto-detected in `reflect_schema.hpp`. When defined, the default `derive_input_schema<T>` walks `std::meta::nonstatic_data_members_of(^^T)` and consumers can delete their specialisations.

## Testing

```bash
cmake -S . -B build/cmake-build
cmake --build build/cmake-build --target euxis-runtime-cpp_tests
./build/cmake-build/libs/runtime/euxis-runtime-cpp_tests
```

148 tests across 22 test suites. The streaming and contracts pilots include `GTEST_SKIP` paths on toolchains that don't ship the relevant feature.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
