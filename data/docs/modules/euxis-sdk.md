# euxis-sdk

The `euxis-sdk` is the official Rust Interface for building Wasm agents for the Euxis Platform.
It contains Rust SDK components for high-performance agent execution and safe systems integration.

## Overview
It provides native abstractions to communicate securely with the Euxis C++ Core via the Model Context Protocol (MCP) using the Extism Plugin Development Kit (PDK).
To maximize performance and memory efficiency, it strictly serializes execution payloads over the Wasm boundary using MessagePack (`rmp-serde`).

## Installation
Add the dependency to your `Cargo.toml`:
```toml
[dependencies]
euxis-sdk = "0.1.0"
```

## Usage
The SDK exposes straightforward, strongly typed MCP operations.

```rust
use euxis_sdk::{get_metrics, sign_payload};

// Retrieve the current mesh metrics
let metrics = get_metrics()?;

// Sign a payload securely using Euxis Cryptographic Provenance
let signature = sign_payload("my-data")?;
```

## Architecture

The SDK conforms strictly to the Euxis Template Conformance rules, separating `core` abstractions from `platform` implementation details:

- **`euxis_sdk::core`**: Defines the shared, platform-agnostic models such as `MCPResult`, `MCPContent`, and strongly typed `Error` boundaries utilizing `thiserror`.
- **`euxis_sdk::platform`**: Provides the concrete implementation (`extism.rs`) that natively wires functions like `call_tool` to the `ExtismHost` bindings.

## Focus Areas

- Low-latency native execution paths
- Tight memory and CPU envelopes with policy ratcheting
- Cross-platform portability through Rust toolchain compatibility
- Strong correctness and safety foundations for security-sensitive integrations

## Quality Controls

- Build manifest: `euxis-sdk/Cargo.toml`
- Stable package gate: `euxis-ops/quality/run_sdk_rust_tests_stable.sh`
- Package policy: `euxis-ops/perf/package_resource_policy.json`
- Workspace excellence gate: `euxis-ops/quality/validate_package_excellence.py`
