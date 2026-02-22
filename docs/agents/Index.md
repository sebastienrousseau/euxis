# Euxis Agents: Architectural Overview

Agents are the atomic units of execution within the Euxis autonomous mesh. **Euxis Agents** are deterministic, cryptographically signed WebAssembly (Wasm) modules. They operate within an ultra-secure, hyper-low latency (v8/Extism) sandbox.

This documentation is the definitive reference for understanding, building, and deploying Euxis Agents.

## 🚀 The Agent Architecture

Euxis operates on a decentralized mesh paradigm. Agents are reactive, ephemeral functions. Invoke them on-demand via the `euxis-core` Gateway.

### Core Principles

1.  **Language Agnostic Execution:** Write agents in Rust, Go, or TypeScript targeting `wasm32-wasi`. The Euxis gateway executes the standardized binary seamlessly.
2.  **Zero-Trust Sandbox:** Execute agents inside a memory-isolated Extism container. They lack access to host filesystems, networks, or OS environments. Explicitly grant permissions via WASI (WebAssembly system interface) mapping.
3.  **Cryptographic Provenance:** Cryptographically sign every agent binary upon compilation via `euxis-cli`. The Gateway verifies this signature via a trusted origin registry before instantiation. This prevents supply-chain poisoning.
4.  **Deterministic Latency:** Spin up agents in low milliseconds utilizing pre-compiled Wasm modules and optimized memory sharing. This enables real-time, event-driven mesh topologies.

## 🛠️ Building an Agent

Create a new Euxis Agent utilizing the CLI scaffolding. First, initialize the boilerplate. Then, implement your execution logic.

### Step 1: Initialization

Initialize the directory structure before implementing your agent logic.

```bash
euxis agent create auth_handler --lang rust
```

This generates a standard directory containing your source code, a `manifest.toml` for metadata, and a standard Makefile.

### Step 2: Implementation

Your core logic must implement the Extism `ExtismPDK` (plugin development kit). Build your execution handler utilizing the `#[plugin_fn]` macro.

**Gotchas:** Ensure your input payload strictly matches the struct signature. Returns an `ExtismError` string if deserialization fails.

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
    let authorized = input.user_token == "super-secret-key";
    
    let output = OutputPayload {
        is_valid: authorized,
        roles: if authorized { vec!["admin".into()] } else { vec![] }
    };

    Ok(Json(output))
}
```

### Step 3: Compilation and Deployment

Compile and package the agent into an `.extism` plugin binary. Deploy the compiled binary to the local Gateway registry.

**Gotchas:** Fails fatally if `wasm32-wasi` is missing from your Rust toolchain.

```bash
# Compile the plugin binary
euxis agent build auth_handler

# Deploy to the Gateway registry
euxis agent deploy auth_handler
```

## 🕸️ The Agent Mesh

Manage inter-agent communication directly through **Mesh Channels** handled by the Euxis Gateway. This eliminates HTTP REST calls. Rely on optimized memory sharing instead to eliminate costly serialization overhead.

### Agent Invocation

Invoke deployed agents via the CLI utilizing specific operational modes.

#### Interactive Mode

Stream the `stdout` and `stderr` capabilities directly to your TUI console.

```bash
euxis auth_handler '{"user_token": "admin_key_123"}'
```

#### Artifact Mode

For continuous integration pipelines, invoke Agents exclusively in Artifact mode. Artifact mode suppresses extraneous logging and console formatting. It yields only the raw JSON or Markdown output block.

```bash
payload=$(euxis auth_handler --artifact-only '{"user_token": "admin_key_123"}')
echo $payload | jq '.is_valid'
```

## 📜 Manifest Specification

Define your agent capabilities cryptographically via the manifest payload.

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

Euxis Agents operate on a strict deny-by-default architecture. Attempting to execute `std::fs::read` from within a Rust Agent triggers a trap (WebAssembly execution halt).

Define mapped virtual paths within the manifest `filesystem` array here to grant host access explicitly.
