# Euxis

The Autonomous Agent Framework.

[![Docs](https://img.shields.io/badge/docs-gh--pages-blue)](https://sebastienrousseau.github.io/euxis/) [![Version][version-badge]][version-url] [![License][license-badge]][license-url]

Version v0.0.2

Use Euxis to deploy, orchestrate, and observe high-performance AI agents. Euxis eliminates the latency and instability of traditional agent loops by utilizing a decentralized WebAssembly (Wasm) mesh.

## Architectural Foundation

Euxis operates on three core principles:

1. **Native Execution**: Agents compile to `wasm32-wasi`. The Euxis Gateway executes them with near-zero cold starts.
2. **Zero-Trust Memory**: Agents run in isolated Extism sandboxes. You must explicitly grant network and filesystem capabilities.
3. **Cryptographic Provenance**: Every agent binary is signed cryptographically to prevent supply-chain poisoning.

## Component Overview

Euxis is modular by design. Integrate exactly what your infrastructure requires.

* `euxis-core`: The central execution engine.
* `euxis-cli`: The command-line orchestrator.
* `euxis-gateway`: The high-throughput HTTP and WebSockets interface.
* `euxis-crypto-lib`: The native Rust cryptography library.
* `euxis-tui`: The terminal user interface for real-time observability.
* `euxis-metrics`: The telemetry and validation framework.

## Documentation Library

Access comprehensive guides to build and scale your agent infrastructure. We maintain 100% documentation coverage across all supported languages.

* [Quick Start](euxis-docs/docs/quick-start.md)
* [User Guide](euxis-docs/docs/guides/user-guide.md)
* [CLI Reference](euxis-docs/docs/reference/cli-reference.md)
* [Module Docs](euxis-docs/docs/modules/index.md)
* [API Reference](euxis-docs/docs/reference/api-reference.md)

## Installation

Instantiate the core packages globally to begin deployment.

```bash
pip install euxis-core euxis-cli euxis-tui
euxis ui
```

## Licensing

AGPL-3.0 License - see [LICENSE](LICENSE).

Author: Sebastien Rousseau

---
*Part of the [Euxis Agent Framework](https://euxis.co) (Euxis v0.0.2)*

[license-badge]: https://img.shields.io/badge/license-AGPL--3.0-blue.svg
[license-url]: LICENSE
[version-badge]: https://img.shields.io/badge/version-v0.0.2-green.svg
[version-url]: https://github.com/sebastienrousseau/euxis/releases/tag/v0.0.2