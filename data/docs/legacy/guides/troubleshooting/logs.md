# Reading Telemetry Logs

Euxis operates asynchronously and concurrently, frequently spawning dozens of WebAssembly modules per second. Reading basic standard output streams (`stdout`) natively becomes unintelligible during federated squad orchestration operations.

Instead, Euxis translates all execution output natively into structured Telemetry Logs. 

## Log Level Configuration

Ensure you explicitly define the verbosity index natively in your Gateway initialization command or global configuration matrix using `log_level` arrays.

* **Debug (`DEBUG`):** Analyzes exact memory pointers natively injected between isolated Wasm buffers. Traces specific HTTP headers. Extremely verbose.
* **Information (`INFO`):** Tracks standard operational orchestration tasks. Notes module instantiation timestamps and completion payloads. (Default)
* **Warning (`WARN`):** Broadcasts structural anomalies natively. For instance, when a WASI capability requests a missing file.
* **Error (`ERROR`):** Identifies fatal Extism traps natively and un-handled authentication failures terminating Gateway execution routes.

## Examining Structural Data

All native logs emit explicitly as structured JSONLines (`.jsonl`) into your explicit Gateway telemetry registry (default: `~/.euxis/euxis-data/gateway/runs/`). 

You can view these payloads safely utilizing standard native JSON processors like `jq`. 

```bash
cat ~/.euxis/euxis-data/gateway/runs/run_broadcast_1771789900646.jsonl | jq '.payload | .is_valid'
```

Alternatively, invoke `euxis ui` to attach locally to the real-time WebSocket logger stream and filter dynamically across running Wasm architectures.

## Analyzing WebAssembly Traps

Extism runtime faults (Traps) indicate severe execution breaches natively. Often, this correlates strictly to missing explicit WASI permissions defined in your JSON Agent Manifest or insufficient dynamic sandbox memory arrays. 

Extract the exact line explicitly in rust execution where the trap occurred using the debug logs trace array and verify your specific `filesystem` mappings constraints matches the requirements identically.
