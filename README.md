# Euxis: High-Performance C++23 Agent OS

[![Docs](https://img.shields.io/badge/docs-gh--pages-blue)](https://sebastienrousseau.github.io/euxis/) [![Version][version-badge]][version-url] [![License][license-badge]][license-url]

Deploy, orchestrate, and observe hardware-native AI agents. Euxis eliminates the latency of traditional Python agent loops by operating as a fully compiled, C++23 Agent OS.

## Core Architectural Requirements

Euxis operates on three uncompromising principles to achieve sub-10ms execution:

1. **Native Execution**: Compile agents to `wasm32-wasi` AOT binaries. The Euxis Gateway executes them within Firecracker microVMs and eBPF Seccomp enclaves for near-zero cold starts.
2. **Zero-Copy Memory**: Map binary snapshots directly into memory via `mmap`. Do not allocate intermediate buffers. Utilize C++23 `std::generator` for lazy-loaded episodic memory streams.
3. **Cryptographic Provenance**: Sign every agent binary cryptographically. The `sentinel-identity` agent enforces strict NHI (Non-Human Identity) IAM scoping and blocks unauthenticated state mutations.

## 2026 Enterprise Mesh

Euxis provides a fully interoperable, state-aware ecosystem:

* **Model Context Protocol (MCP)**: Discover and bind to universal tools dynamically via the `McpClient`.
* **SIMD-Aware Orchestration**: Broadcast `BidRequests` to the mesh. The `FinOpsRouter` evaluates 16+ providers simultaneously using Structure-of-Arrays (SoA) SIMD auto-vectorization.
* **Proactive Self-Healing**: Deploy the `SupervisorAgent` daemon to monitor circuit breakers and autonomously tune LRU eviction policies, mitigating context window blowout.

## System Topology

Euxis enforces a strict Domain-Driven Design (DDD) layout. Integrate exactly what your infrastructure requires.

### Application Layer (`cmd/`)

* `cli`: Command-line orchestrator and TUI.
* `gateway`: High-throughput asynchronous WebSockets and HTTP interface.
* `etx`: Qt6 desktop GUI featuring 17 screens and command palette.
* `publisher`: C++23 `inja` rendering engine for low-latency document generation.

### SDK Layer (`pkg/`)

* `core`: Central execution engine, `FinOpsRouter`, and `SwarmOrchestrator`.
* `network`: `McpClient`, asynchronous A2A transports, and thread-safe resilience patterns.
* `runtime`: Agent lifecycle, scheduling, and MessagePack zero-copy memory stores.
* `crypto`: AES-256-GCM, Ed25519, and Argon2id primitives.
* `identity`: W3C DID, Verifiable Credentials, and ERC-8004 agent cards.
* `inference`: Hardware-accelerated local inference via `llama.cpp`.
* `bridge`: Skill import, admission pipeline, and sandbox execution.
* `memory`: Tier-bound encrypted memory with AAD isolation.
* `metrics`: Telemetry, performance profiling, and validation framework.
* `a2a`: A2A v0.2 protocol and JSON-RPC message formats.
* `security`: Threat detection and policy enforcement.
* `adapters`: Omnichannel endpoints (WhatsApp, Discord, Slack, Telegram).
* `bench`: Security, autonomy, performance, and interop benchmarks.

### Internal System (`internal/`)

* `platform`: Platform Abstraction Layer (PAL). Hardware-specific OS integrations (macOS, Linux, WSL).

## Building

Build the Agent OS from source.

* **Precondition**: Ensure CMake 3.28+ and a C++23-capable compiler (GCC 14+ / Clang 18+) are present.
* **Postcondition**: Generates highly optimized binaries in `build/cmake-build`.

```bash
make cpp-configure
make cpp-build
make cpp-test
```

## Licensing

AGPL-3.0 License - see [LICENSE](LICENSE).

Author: Sebastien Rousseau

---
*Part of the [Euxis Agent Framework](https://euxis.co) (Euxis v0.0.3)*

[license-badge]: https://img.shields.io/badge/license-AGPL--3.0-blue.svg
[license-url]: LICENSE
[version-badge]: https://img.shields.io/badge/version-v0.0.3-green.svg
[version-url]: https://github.com/sebastienrousseau/euxis/releases/tag/v0.0.3