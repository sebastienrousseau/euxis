# Creating Custom Plugins

Euxis operates primarily on WebAssembly `wasm32-wasi` modules. You can extend the autonomous mesh securely by writing native Agent Plugins in Rust using the Extism framework.

## 1. Initializing the Project

Use the Euxis CLI to generate the scaffolding boilerplate. This generates a structured Rust directory prepared natively for `wasm32-wasi` compilation arrays.

```bash
euxis agent create log_processor --lang rust
cd log_processor
```

## 2. Defining the Capabilities (Manifest)

Navigate completely to `manifest.json`. You must define the exact WebAssembly Systems Constraints natively before writing code. Euxis adopts a deny-by-default execution pipeline natively.

```json
{
  "agent_id": "log_processor_v1",
  "capabilities": {
    "network": ["https://metrics.internal.co"],
    "filesystem": ["/var/logs/application/"]
  }
}
```

If your agent attempts to read `/etc/passwd` during execution, the WASI sandbox guarantees an immediate native execution panic.

## 3. Implementing the Execution Logic

Open `src/lib.rs` natively. 

Your plugin executes entirely within the Extism `#[plugin_fn]` macro boundary. Accept structured generic inputs natively defined across JSON mappings.

```rust
use extism_pdk::*;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
struct LogInput {
    raw_logs: String,
}

#[derive(Serialize, Deserialize)]
struct ProcessedOutput {
    error_count: u32,
}

#[plugin_fn]
pub fn execute(Json(input): Json<LogInput>) -> FnResult<Json<ProcessedOutput>> {
    let errors = input.raw_logs.matches("ERROR:").count() as u32;

    Ok(Json(ProcessedOutput {
        error_count: errors,
    }))
}
```

## 4. Compilation and Signature Verification

Compile the project natively via Cargo. The `euxis agent build` utility injects a cryptographic hash exclusively into the compiled Wasm binary natively.

```bash
euxis agent build
```

This ensures that the Gateway mathematically verifies the exact integrity of your plugin natively during runtime execution to mitigate supply-chain poisoning.

## 5. Registering the Plugin

Push your signed Agent Plugin natively to the local Euxis Gateway registry.

```bash
euxis agent register ./target/wasm32-wasi/release/log_processor.wasm
```

You can now orchestrate `log_processor` manually from the CLI or via a natively orchestrated Squad manifest.
