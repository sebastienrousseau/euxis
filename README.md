# Euxis

The Autonomous Agent Framework.

[![Docs](https://img.shields.io/badge/docs-gh--pages-blue)](https://sebastienrousseau.github.io/euxis/) [![Version][version-badge]][version-url] [![License][license-badge]][license-url]

Version v0.0.3

Use Euxis to deploy, orchestrate, and observe high-performance AI agents. Euxis eliminates the latency and instability of traditional agent loops by utilizing a decentralized WebAssembly (Wasm) mesh.

## Architectural Foundation

Euxis operates on three core principles:

1. **Native Execution**: Agents compile to `wasm32-wasi`. The Euxis Gateway executes them with near-zero cold starts.
2. **Zero-Trust Memory**: Agents run in isolated Extism sandboxes. You must explicitly grant network and filesystem capabilities.
3. **Cryptographic Provenance**: Every agent binary is signed cryptographically to prevent supply-chain poisoning.

## 2026 Evolution

Euxis has evolved into a fully interoperable and omnichannel framework:

- **Model Context Protocol (MCP)**: Native host support for universal tool and context sharing.
- **Agent Swarms**: Declarative playbook orchestration for multi-agent collaboration.
- **FinOps Router**: Score-based model provider selection to balance cost, speed, and reliability.
- **Device Nodes**: Control local hardware and browsers securely from sandboxed agents.
- **Omnichannel Presence**: Integrated adapters for WhatsApp, Discord, Slack, and Telegram.

## Component Overview

Euxis is modular by design. The runtime is built entirely in C++23 across 16 modules. Integrate exactly what your infrastructure requires.

### C++23 Modules

* `euxis-core-cpp`: The central execution engine and Extism WASM runtime.
* `euxis-cli-cpp`: The command-line orchestrator.
* `euxis-gateway-cpp`: The high-throughput HTTP and WebSockets interface.
* `euxis-metrics-cpp`: The telemetry and validation framework.
* `euxis-crypto-cpp`: AES-256-GCM, Ed25519, Argon2id key derivation via libsodium.
* `euxis-bridge-cpp`: Skill import, static analysis, admission pipeline, sandbox execution.
* `euxis-memory-cpp`: Tier-bound encrypted memory with AAD isolation.
* `euxis-identity-cpp`: W3C DID, Verifiable Credentials, ERC-8004 agent cards.
* `euxis-inference-cpp`: llama.cpp + Ollama inference, model registry, quality gate.
* `euxis-a2a-cpp`: A2A v0.2 protocol, JSON-RPC server, HTTP transport.
* `euxis-security-cpp`: Threat detection, policy enforcement, and sandbox auditing.
* `euxis-adapters-cpp`: Omnichannel adapters for WhatsApp, Discord, Slack, and Telegram.
* `euxis-runtime-cpp`: Agent lifecycle, scheduling, and resource management.
* `euxis-scripts-cpp`: Build and deployment automation utilities.
* `euxis-bench-cpp`: Security, autonomy, performance, portability, interop benchmarks.
* `euxis-etx`: Qt6 desktop GUI with 17 screens, 3 themes, and command palette.

## Building

Build all C++23 modules from source. Requires CMake 3.28+, a C++23-capable compiler, and vcpkg.

```bash
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