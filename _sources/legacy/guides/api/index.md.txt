# Euxis API Reference

The Euxis framework exposes granular, high-throughput interfaces across multiple languages and protocols. Use this centralized registry to integrate your infrastructure with the Euxis autonomous mesh.

## Core Interfaces

Euxis operates primarily through specialized gateways and native library bindings. Select your integration path based on your latency and abstraction requirements.

* **[Gateway HTTP/REST API](gateway.md):** The primary integration interface for remote or cross-network agent orchestration.
* **[Gateway WebSockets](websockets.md):** The ultra-low latency, bidirectional event stream for real-time observability and mesh telemetry.
* **[CLI Commands (`euxis-cli`)](cli.md):** The native terminal commands for deployment, manifest generation, and debugging.
* **[WASI Manifests (JSON)](manifests.md):** The strict JSON schema for defining agent capabilities and virtual filesystem bindings.

## Native Bindings

For maximum throughput and 0-copy serialization, embed Euxis directly into your runtime via our supported package libraries.

* **[Python (`euxis-core`)](python/index.md):** The native Python orchestrator and dependency manager bindings.
* **[Rust (`crypto-lib`)](../euxis-crypto/docs/index.md):** Highly deterministic, memory-safe cryptographic primitives embedded via `PyO3`.

## Rate Limiting & Authentication

All API endpoints enforcing state mutations or deploying agents require cryptographic authentication. Consult the **[Security Guide](../euxis-policy/SECURITY_INTEGRATION.md)** before exposing your Gateway instance to public networks.
