# Euxis Agents: Architectural Overview

Agents are the atomic units of execution within the Euxis autonomous mesh. Unlike traditional scripts or monolithic services, **Euxis Agents** are deterministic, cryptographically signed WebAssembly (Wasm) modules that operate within an ultra-secure, hyper-low latency (v8/Extism) sandbox.

This documentation serves as the definitive reference for understanding, building, and deploying Euxis Agents, fulfilling [GitHub Issue #22](https://github.com/sebastienrousseau/euxis/issues/22).

---

## 🚀 The Agent Architecture

Euxis operates on a decentralized "mesh" paradigm. Agents are not persistently running servers; they are reactive, ephemeral functions invoked on-demand by the `euxis-core` Gateway.

### Core Principles

1.  **Language Agnostic Execution:** Write agents in Rust, Go, TypeScript, C++, or any language that compiles down to `wasm32-wasi`. The Euxis gateway executes the standardized binary without requiring the host environment to possess the corresponding toolchain.
2.  **Zero-Trust Sandbox:** Each agent executes inside a memory-isolated Extism container. Agents *cannot* access the host filesystem, network, or OS environment variables unless explicitly mapped via standard WASI capabilities during invocation.
3.  **Cryptographic Provenance:** Every agent binary is signed cryptographically upon compilation by the `euxis-cli`. The Gateway verifies this signature via a trusted origin registry before instantiation, preventing supply-chain poisoning.
4.  **Deterministic Latency:** By utilizing pre-compiled Wasm modules and optimized memory sharing, agent spin-up times are measured in low milliseconds, enabling real-time, event-driven mesh topologies.

## 🛠️ Building an Agent

Creating a new Euxis Agent involves utilizing the Euxis CLI scaffolding commands. 

### Step 1: Initialization

```bash
# Initialize a new Rust-based Wasm agent
euxis agent create auth_handler --lang rust
```

This generates a boilerplate directory containing the source code, a `manifest.toml` for metadata, and a Makefile for generic compilation steps.

### Step 2: Implementation (Rust Example)

The core logic must implement the Extism ExtismPDK interface.

```rust
use extism_pdk::*;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
struct InputPayload {
    user_token: String,
}

#[derive(Serialize, Deserialize)]
struct OutputPayload {
    is_valid: bool,
    roles: Vec<String>,
}

#[plugin_fn]
pub fn execute(Json(input): Json<InputPayload>) -> FnResult<Json<OutputPayload>> {
    // Agent execution logic here
    let authorized = input.user_token == "super-secret-key";
    
    let output = OutputPayload {
        is_valid: authorized,
        roles: if authorized { vec!["admin".into()] } else { vec![] }
    };

    Ok(Json(output))
}
```

### Step 3: Compilation and Deployment

```bash
# Compile and package the agent into an .extism plugin binary
euxis agent build auth_handler

# Deploy the binary to the local Gateway registry
euxis agent deploy auth_handler
```

## 🕸️ The Agent Mesh

Agents communicate with each other not via HTTP REST calls, but through **Mesh Channels** managed by the Euxis Gateway. This inter-agent communication utilizes optimized memory sharing when possible, preventing costly serialization overhead.

### Agent Invocation via CLI

You can directly interact with deployed agents using the CLI. 

#### Interactive Mode
When running interactively, the CLI streams the stdout and stderr capabilities directly to your TUI console.

```bash
euxis auth_handler '{"user_token": "admin_key_123"}'
```

#### Artifact Mode (`--artifact-only`)
When utilized within automation scripts or continuous integration pipelines, Agents should be invoked in Artifact mode. This suppresses extraneous logging and console formatting, yielding only the raw JSON or Markdown output block emitted by the Agent's Extism module.

```bash
payload=$(euxis auth_handler --artifact-only '{"user_token": "admin_key_123"}')
echo $payload | jq '.is_valid'
```

## 📜 Agent Manifest Specification

Every compiled Agent possesses a cryptographic manifest. Below is an example payload mapped by the Gateway.

```json
{
  "agent_id": "auth_handler_v1.0.2",
  "wasm_hash": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
  "capabilities": {
    "network": ["https://api.oauth.provider.com"],
    "filesystem": []
  },
  "author": "Euxis Core Team"
}
```

## 🛡️ Security Posture

Euxis Agents operate strictly on a deny-by-default architecture. Attempting to execute `std::fs::read` from within a Rust Agent will result in an immediate trap error unless the `filesystem` array within the manifest explicitly defines a mapped virtual path allowed by the Host Gateway.
