# What is an Agent?

Agents represent the atomic execution unit within Euxis. Unlike traditional AI integration frameworks that map strings to generic Python functions, Euxis Agents operate as cryptographically signed, pre-compiled WebAssembly (Wasm) modules.

## The WebAssembly Runtime Constraint

Euxis operates on the philosophy of zero-trust. All agent code execution (including standard libraries for networking, filesystem manipulation, and state access) triggers instantly inside an **Extism** runtime container.

1. **Isolation:** Native agents cannot breach their WebAssembly memory sandbox.
2. **Speed:** Sandboxed execution guarantees millisecond instantiation times natively on `AArch64` and `amd64`.
3. **Universality:** Write agents in any language capable of emitting `wasm32-wasi` (e.g., Rust, Go, C++, Zig).

## Extism Plugins

An agent acts as a reactive Extism plugin executing `#[plugin_fn]` callbacks. Whenever the Gateway or CLI pushes intent into the agent, the container wakes up, parses the binary buffer into memory, executes its logic, and returns a zero-copy response back to the orchestrator layer.

## Capability Manifests

To execute standard logic, such as reading a project folder, the Euxis CLI orchestrator injects explicit **Capabilities** mapped through to the WebAssembly System Interface (WASI).

For example, an agent tasked with debugging your code possesses zero filesystem access unless the Gateway explicitly injects a mapping of its working directory.

This architecture protects your machine. If a malicious user tricks a remote agent into reading your `/etc/shadow` file, the memory violation instantly traps the execution and crashes the agent.
