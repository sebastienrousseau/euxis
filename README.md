<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<h1 align="center">euxis</h1>

<p align="center">
  An ultra-fast, automated security auditor for your codebase.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?branch=main&style=for-the-badge&logo=github" alt="Build" /></a>
  <a href="https://github.com/sebastienrousseau/euxis/releases"><img src="https://img.shields.io/github/v/release/sebastienrousseau/euxis?include_prereleases&style=for-the-badge&color=fc8d62" alt="Release" /></a>
  <a href="https://sebastienrousseau.github.io/euxis/"><img src="https://img.shields.io/badge/Docs-gh--pages-66c2a5?style=for-the-badge" alt="Docs" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-AGPL--3.0-blue.svg?style=for-the-badge" alt="License: AGPL-3.0" /></a>
  <a href="https://euxis.co"><img src="https://img.shields.io/badge/euxis.co-Project%20site-orange?style=for-the-badge" alt="Project site" /></a>
</p>

---

## What this actually is

**Euxis verifies code the way a human security auditor would — except in under three minutes — and ships a Sigstore-signed evidence pack any regulator can verify offline.**

When a software team needs to prove their app meets SOC 2, ISO 27001, NIST, OWASP, or any of fourteen other frameworks, they normally pay a human expert to spend weeks reading code, running checks, and writing reports. Euxis is a single binary you drop into your CI pipeline: every time the code changes, it runs the same audit a human would, and emits a tamper-proof, regulator-ready evidence pack.

The evidence pack is a **Sigstore Bundle v0.3** — an in-toto Statement v1 wrapped in a SLSA v1.2 provenance predicate, DSSE-signed with Ed25519, and bundled with the SARIF findings, CycloneDX 1.6 + SPDX 3.0.1 SBOMs, and OpenVEX exception document. A verifier (auditor, regulator, SOC) checks the chain offline with `euxis verify` or any sigstore-compatible tool — no contact to a vendor service required. See [`docs/evidence-packs/anatomy.md`](docs/evidence-packs/anatomy.md) for the full anatomy and [`docs/evidence-packs/verifying-a-bundle.md`](docs/evidence-packs/verifying-a-bundle.md) for the verification run-book.

The same engine ships as an embeddable C++23 SDK across sixteen libraries, so teams building agentic verification systems can wire it directly into their own product — see [Embedding euxis in C++](#embedding-euxis-in-c) below.

## What the jargon actually means

| Phrase you'll see | What it actually means |
|---|---|
| **Cryptographically-signed evidence pack** | A tamper-proof digital receipt. Guarantees to regulators that the audit results are real and haven't been faked. Re-run the same audit next quarter against the same code: same bytes, same digest. |
| **Native speed / C++23 static binary** | One clean binary that runs instantly. No Python runtime to warm up, no per-turn JIT lag, no interpreter in the agent loop. |
| **Swarm of agents** | A group of AI assistants working simultaneously to read and test different parts of your code, coordinated by a single controller. |
| **FinOps router** | A smart budget manager. Routes easy tasks to cheap models (Ollama, Haiku) and hard tasks to expensive ones (Opus, GPT-5), so you don't overpay. |
| **A2A v0.2 agent meshes** | A standardised wire protocol that lets independent agents trade tasks. Open spec; not Euxis-specific. |
| **CI pipeline integration** | Drop it into GitHub Actions / GitLab CI / Jenkins. Runs every time someone pushes code. Fails the build if a compliance gate breaks. |
| **Forensic mode** | The deepest review mode (`euxis review --forensic`). Adds supply-chain audit, dependency provenance, and admission-policy enforcement. ~3 minutes vs ~45 seconds for triage. |

## How you use it

```bash
euxis triage .                              # 45-second bounded scan
euxis check .                               # standard verification, ~3 min
euxis review . --forensic                   # forensic depth + supply-chain audit
euxis certify-readiness . --framework soc2  # SOC 2 readiness report

euxis sbom . --format=both                  # CycloneDX 1.6 + SPDX 3.0.1
euxis slopsquatting .                       # LLM-hallucinated-package guard
euxis attest evidence.tar.gz                # Sigstore-signed evidence bundle
euxis verify evidence.tar.gz.sigstore.json  # offline signature check
euxis cache stats                           # incremental scan cache
```

Pipe the output into your CI gate, or hand the evidence pack to a regulator. See [`docs/compliance/cra.md`](docs/compliance/cra.md) for the EU Cyber Resilience Act mapping and [`docs/compliance/dora.md`](docs/compliance/dora.md) for DORA / TLPT.

---

## Contents

**Getting started**

- [Install](#install) — prerequisites, build, verify
- [Quick start](#quick-start) — first verification in five minutes

**Command surface**

- [Core commands](#core-commands) — daily-driver verbs
- [Command groups](#command-groups) — the full eight groups

**SDK / Library reference**

- [Embedding euxis in C++](#embedding-euxis-in-c) — the SDK example
- [Public libraries](#public-libraries) — sixteen-library matrix
- [Build options](#build-options) — CMake feature flags
- [Provider strategy](#provider-strategy) — FinOps router defaults

**Evidence and compliance**

- [Anatomy of an evidence pack](docs/evidence-packs/anatomy.md) — bundle layout, top to bottom
- [Verifying a bundle](docs/evidence-packs/verifying-a-bundle.md) — operational run-book
- [EU Cyber Resilience Act mapping](docs/compliance/cra.md) — CRA Article 13 / 14
- [DORA / TLPT mapping](docs/compliance/dora.md) — financial-services workflow
- [Benchmark methodology](docs/benchmarks/methodology.md) — how the scan + evidence numbers are measured

**Operational**

- [Architecture](#architecture) — directory layout at a glance
- [Why this approach?](#why-this-approach) — design rationale
- [Building from source](#building-from-source) — make targets
- [Benchmarks](#benchmarks) — two harnesses, two jobs
- [Documentation](#documentation) — all reference docs
- [Contributing](#contributing)
- [License](#license)

---

## Install

Euxis builds from source. Pre-built packages are not yet published; the build is one command on macOS, Linux, and WSL2.

### Prerequisites

| Tool | Minimum | Check |
|---|---|---|
| CMake | 3.28 | `cmake --version` |
| C++ compiler | GCC 14+ or Clang 18+ | `g++ --version` or `clang++ --version` |
| Git | 2.x | `git --version` |
| libsodium | 1.0.18+ | `pkg-config --modversion libsodium` |
| SQLite | 3.x | `sqlite3 --version` |

Optional, only when the corresponding feature is enabled:

| Tool | Required by | Install |
|---|---|---|
| Qt6 | `apps/etx` desktop GUI | `brew install qt` / `pacman -S qt6-base` |
| Doxygen | `EUXIS_BUILD_DOCS=ON` (default ON) | `pacman -S doxygen graphviz` |
| Google Benchmark | fetched automatically when `EUXIS_BUILD_GBENCH=ON` | — |

### Platform setup

| Platform | Command |
|---|---|
| macOS (Homebrew) | `brew install cmake gcc libsodium sqlite` |
| Ubuntu / Debian / WSL2 | `sudo apt update && sudo apt install -y cmake g++-14 git libsodium-dev libsqlite3-dev` |
| Arch / CachyOS | `sudo pacman -S cmake gcc git libsodium sqlite` |

### Build

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
cd ~/.euxis
make cpp-build              # Release + LTO, parallel
make cpp-test               # ctest, all suites

sudo ln -sf ~/.euxis/cmake-build/apps/cli/euxis-cli /usr/local/bin/euxis
```

### Verify

```bash
euxis doctor
```

---

## Quick start

```bash
euxis triage .                              # 45-second bounded triage
euxis check .                               # standard verification, ~3 min
euxis review . --forensic                   # forensic depth + supply-chain audit
euxis certify-readiness . --framework soc2  # SOC 2 readiness report
euxis compare .                             # triage vs deep, side-by-side
euxis stats --last 5                        # recent metrics + drift history
```

The triage path is bounded for CI. The full review path is bounded for human review. The certification path emits a reproducible evidence pack.

---

## Core commands

The Core group is the daily-driver surface. Aliases map to the canonical command.

| Command | Alias | What it does |
|---|---|---|
| `euxis check [target]` | — | Standard verification. The default mode. |
| `euxis triage [target]` | `euxis quick` | Fast bounded triage, ~45 seconds. |
| `euxis review [target]` | `euxis deep` | Deep verification, standard or `--forensic`. |
| `euxis certify-readiness [target] --framework <name>` | — | Certification readiness across 18 domains. |
| `euxis compare <target>` | — | Diff triage results against deep verification. |
| `euxis stats` | `euxis metrics` | Validation metrics + drift history. |
| `euxis policy <subcommand>` | — | Policy inspection and enforcement. |
| `euxis playbook <name>` | `euxis pb` | Run a multi-step verification pipeline. |
| `euxis combo run <pipeline> "<goal>"` | — | Multi-agent execution graph. |
| `euxis doctor` | `euxis diag` | Environment diagnostics. |

---

## Command groups

Euxis ships sixty commands across eight groups: **Core, Lifecycle, System, Fleet, Knowledge, Infrastructure, Development, Specialized**. The full inventory lives in [`docs/reference/cli-reference.md`](docs/reference/cli-reference.md); `euxis --help` enumerates every command and flag at the terminal.

| Group | Headline commands |
|---|---|
| Core | `check`, `triage`, `review`, `certify-readiness`, `compare`, `stats`, `policy`, `playbook`, `doctor` |
| Lifecycle | session and run-state management |
| System | host inspection and configuration |
| Fleet | agent roster, routing, and assignment |
| Knowledge | corpus and reference data |
| Infrastructure | gateway and deployment surfaces |
| Development | development-only helpers (lint, fmt, format-check) |
| Specialized | targeted workflows for specific frameworks |

---

## Embedding euxis in C++

The same libraries the CLI uses ship as a public SDK under `libs/`. The minimal end-to-end example lives at [`docs/examples/cpp/a2a_minimal_server/`](docs/examples/cpp/a2a_minimal_server/) and builds an A2A v0.2 server handler in under 100 lines of C++23.

```bash
cmake -B cmake-build -DEUXIS_BUILD_EXAMPLES=ON
cmake --build cmake-build --target euxis_example_a2a_minimal_server
./cmake-build/docs/examples/cpp/a2a_minimal_server/euxis_example_a2a_minimal_server
```

Expected output is the full A2A task lifecycle: `agent/card`, `capabilities/list`, `task/create`, `task/get`, `task/cancel`, `task/get` again. See the example's own `README.md` for the annotated walk-through.

---

## Public libraries

Sixteen static libraries ship from `libs/`. Link only what the application uses.

| Library | Public header | Purpose |
|---|---|---|
| `euxis-a2a-cpp` | `<euxis/a2a/agent_card.hpp>` | A2A v0.2 protocol — cards, tasks, JSON-RPC server |
| `euxis-a2a-types-cpp` | `<euxis/a2a/message.hpp>` | Shared message + transport types |
| `euxis-runtime-cpp` | `<euxis/runtime/agent_session.hpp>` | Agent session, lifecycle, tool registry |
| `euxis-core-cpp` | `<euxis/core/contracts.hpp>` | FinOps router, supervisor, swarm orchestrator |
| `euxis-crypto-cpp` | `<euxis/crypto/aes_gcm.hpp>` | AES-256-GCM, Ed25519, BLAKE2b key derivation |
| `euxis-identity-cpp` | `<euxis/identity/did.hpp>` | DID, ERC-8004 attestation, credentials |
| `euxis-network-cpp` | `<euxis/network/mcp_client.hpp>` | MCP client, WebSocket transport, resilience |
| `euxis-bridge-cpp` | `<euxis/bridge/admission.hpp>` | External-tool admission, audit, verification |
| `euxis-metrics-cpp` | `<euxis/metrics/analyzer.hpp>` | mdspan telemetry, validation pipeline |
| `euxis-memory-cpp` | `<euxis/memory/store.hpp>` | SQLite-backed persistent memory |
| `euxis-inference-cpp` | `<euxis/inference/engine.hpp>` | Local inference via llama.cpp, ollama |
| `euxis-adapters-cpp` | `<euxis/adapters/adapter.hpp>` | Outbound channel adapters (Slack, Discord, Telegram) |
| `euxis-security-cpp` | `<euxis/security/errors.hpp>` | Threat detection, policy enforcement |
| `euxis-platform-cpp` | `<euxis/platform/platform.hpp>` | OS abstraction (macOS, Linux, WSL2) |
| `euxis-publisher-cpp` | `<euxis/publisher/publisher.hpp>` | Document rendering and export |
| `euxis-bench-cpp` | `<euxis/bench/runner.hpp>` | Benchmark harness and result matrix |

The full API reference is generated by `cmake --build cmake-build --target docs` (requires `doxygen`; falls back to a no-op when absent). Output lands in `cmake-build/docs/html/` styled with [doxygen-awesome-css](https://jothepro.github.io/doxygen-awesome-css/).

---

## Build options

All optional features are off by default unless noted. Enable only what the application needs.

| Option | Default | What it enables |
|---|---|---|
| `EUXIS_BUILD_EXAMPLES` | `OFF` | First-party SDK examples under `docs/examples/cpp/` |
| `EUXIS_BUILD_GBENCH` | `OFF` | Google Benchmark statistical harness for the `performance` suite |
| `EUXIS_BUILD_DOCS` | `ON` | Doxygen API reference target (auto-skips if doxygen absent) |
| `EUXIS_NATIVE_ARCH` | `OFF` | Use `-march=native` for dev builds |
| `EUXIS_COVERAGE` | `OFF` | Enable gcov instrumentation |
| `EUXIS_DISABLE_SANITIZERS` | `OFF` | Skip ASan/UBSan in debug builds |

```bash
# Example: examples + statistical bench + documentation
cmake -B cmake-build \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DEUXIS_BUILD_EXAMPLES=ON \
  -DEUXIS_BUILD_GBENCH=ON
```

---

## Provider strategy

Euxis routes inference work to the optimal provider based on task class. The mapping is configurable per environment via [`data/config/provider_strategy.json`](data/config/provider_strategy.json) and overridable at runtime through environment variables.

| Task class | Default provider | Fallbacks |
|---|---|---|
| Research / synthesis | OpenAI | Gemini, Claude |
| Coding / architecture / audit | Claude | Gemini, Ollama |
| Deep research / security | Gemini | OpenAI, Claude |
| Private / local | Ollama | — |
| Surgical edits | Aider | Claude, Ollama |
| Terminal automation | Kiro | ShellGPT, Claude |

Environment overrides: `EUXIS_DEFAULT_RESEARCH_PROVIDER`, `EUXIS_DEFAULT_CODING_PROVIDER`, `EUXIS_DEFAULT_SECURITY_PROVIDER`.

The `FinOpsRouter` in [`libs/core/src/router.cpp`](libs/core/src/router.cpp) picks within each class using a Structure-of-Arrays scoring loop — branchless, SIMD-vectorizable, deterministic. The `swarm` priority cycles through providers via an atomic round-robin counter.

---

## Architecture

```
apps/                Application layer
  cli/               Command-line interface — 60 commands, 8 groups
  etx/               Qt6 desktop GUI — 17 screens
  gateway/           HTTP/WebSocket server
  publisher/         Document rendering engine

libs/                SDK layer — 16 static libraries
  a2a/               A2A v0.2 protocol implementation
  a2a-types/         Shared message and transport types
  adapters/          Outbound channel adapters
  bench/             Benchmark harnesses (custom + optional Google Benchmark)
  bridge/            CLI bridge for external-tool execution + admission
  core/              FinOps router, supervisor, swarm orchestrator
  crypto/            AES-256-GCM, Ed25519, BLAKE2b, Argon2id
  identity/          DID, ERC-8004 attestation, credential management
  inference/         Local inference via llama.cpp, ollama
  memory/            Persistent memory store, SQLite-backed
  metrics/           mdspan telemetry, fast collector, validation pipeline
  network/           MCP client, WebSocket transport, resilience patterns
  platform/          OS abstraction (macOS, Linux, WSL2)
  publisher/         Document export
  runtime/           Agent session, lifecycle, tool manifest, validator
  security/          Threat detection, policy enforcement, errors

data/                Configuration, agent prompts, playbooks, docs
docs/                Documentation site sources + SDK examples
```

---

## Why this approach?

Euxis is a multi-agent code certification CLI built native, not a wrapper around an interpreted runtime. The implementation is C++23 throughout — header files use `std::expected` for error handling (61 sites across `libs/`), `std::mdspan` for metric collection, and concepts for adapter interfaces. The same binary runs the entire agent loop in-process: no Python startup, no JIT warm-up, no per-turn marshalling cost.

Three architectural choices motivate the rewrite from interpreted equivalents:

1. **Cryptographic provenance is in the hot path.** Every verification carries an Ed25519 attestation. The AES-256-GCM context (`libs/crypto/aes_gcm.hpp`) caches the key schedule once and amortizes it across calls — measured at 1.54 GiB/s on x86_64-v3 versus 1.50 GiB/s for the simple API, with both well above the 50,000 ops/sec SLO target. Interpreted runtimes pay the schedule per call.

2. **A2A is first-class, not glued on.** `libs/a2a` implements the v0.2 protocol surface end-to-end — agent cards, validation, JSON-RPC server, HTTP and WebSocket transports, msgpack binary serialization at 30 M ops/sec. The minimum-viable embedding is the SDK example at `docs/examples/cpp/a2a_minimal_server/main.cpp`.

3. **No dynamic plugin loading.** Every library under `libs/` is statically linked into the apps that use it. Configuration changes never require relinking, but the *capability surface* of any euxis binary is determined entirely at build time. This is a feature for an audit tool: builds are deterministic and reproducible, and a verification cannot be silently extended by a runtime-loaded plugin.

The trade-off is that euxis does not match the plugin-loader ergonomics of personal-AI-assistant frameworks (OpenClaw, Hermes Agent). That is intentional — the product is verification, not extensibility.

---

## Building from source

```bash
make cpp-configure    # CMake configure (Release, LTO, _FORTIFY_SOURCE=3)
make cpp-build        # Build all targets
make cpp-test         # Run test suite (ctest)
make cpp-clean        # Remove build artifacts
make cpp-format       # clang-format pass
make cpp-coverage     # Build with coverage (requires gcovr, lcov)
make cpp-clang-tidy   # Static analysis
```

Override parallelism with `make cpp-build CPP_BUILD_JOBS=8`. The repository pins `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` to accommodate vendored third-party CMake files that predate the modern minimum-version policy.

---

## Benchmarks

Two complementary harnesses ship with euxis. See [`libs/bench/README.md`](libs/bench/README.md) for the full guide.

| Harness | Use for | Output |
|---|---|---|
| Custom (`euxis-bench-cpp`) | CI smoke tests against hardcoded SLO targets across five suites | JSON via `ResultMatrix` |
| Google Benchmark (`euxis_perf_gbench`, opt-in) | Statistical analysis of the `performance` suite; trend tracking | `benchmark::` console / JSON / CSV |

Initial run on GCC 16 / x86_64-v3 (`euxis_perf_gbench --benchmark_min_time=0.1s`):

| Benchmark | Time | Throughput |
|---|---|---|
| `BM_CryptoThroughput_Simple` | 0.640 us | 1.50 GiB/s |
| `BM_CryptoThroughput_Cached` | 0.622 us | 1.54 GiB/s |
| `BM_KeyDerivation_FastPath` | 0.203 us | — |
| `BM_AgentCardMsgpackRoundTrip` | 33.1 ns | 30.3 M ops/sec |

Synthetic microbenchmarks measure library overhead, not user-visible performance. Before optimizing a hot path that a bench identifies, profile a real `euxis-cli` workload (`perf record euxis-cli triage .`) and confirm the function appears in the production call graph.

---

## Documentation

| Guide | Content |
|---|---|
| [Quick start](docs/essentials/quick-start.md) | Clone to verified, in five minutes |
| [User guide](docs/guides/user-guide.md) | Complete CLI reference and modes |
| [CLI reference](docs/reference/cli-reference.md) | Every command, flag, and example |
| [Fleet guide](docs/guides/fleet-guide.md) | Agent roster and routing |
| [Bench harnesses](libs/bench/README.md) | Custom vs. Google Benchmark, when to use which |
| [SDK example](docs/examples/cpp/a2a_minimal_server/README.md) | Minimal A2A v0.2 server |
| API reference | Build locally: `cmake --build cmake-build --target docs` |

Hosted documentation lives at [sebastienrousseau.github.io/euxis](https://sebastienrousseau.github.io/euxis/).

---

## Contributing

Issues and pull requests are welcome. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the development setup, commit-signing policy, and review process. All commits must be cryptographically signed.

For sweeping changes (formatting passes, warning-flip drives, coverage drives), open an issue first to align on scope before sending a PR.

Security disclosures: see [`SECURITY.md`](SECURITY.md) for the coordinated-disclosure policy.

---

## License

AGPL-3.0 — see [LICENSE](LICENSE).

Euxis v0.1.2 · [euxis.co](https://euxis.co)
