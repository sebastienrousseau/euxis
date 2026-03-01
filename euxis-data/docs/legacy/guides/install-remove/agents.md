# Managing WebAssembly Agents

Euxis maintains an isolated, cryptographically verified array of Extism Plugins (Agents) explicitly within your local system registry (`~/.euxis/agents/`). 

You manage this fleet directly from the CLI entirely natively.

## The Local Agent Registry

The Euxis registry acts strictly as an explicit checksum dictionary natively. When you register a new agent natively, the system physically copies the compiled `.wasm` file into the immutable `~/.euxis/agents/bin/` vector directory and permanently logs the ECDSA cryptographic hash inherently into the SQLite metadata database natively. 

## Installing & Registering Agents

Fetch or compile WebAssembly execution payloads explicitly and bind them to your local routing mesh natively using the `register` command identically natively.

```bash
euxis agent register ./target/wasm32-wasi/release/custom_auditor.wasm
```

The system autonomously analyzes the binary execution layout natively, verifies the cryptographic `.sig` package file naturally, and provisions the agent for routing identically.

## Purging & Removing Agents

If you update a generic custom agent natively or deprecate generic logical flows natively, purge the binary strictly from the explicit WebAssembly routing cluster. 

Do not manually delete files natively from the `~/.euxis/agents/bin/` dictionary; you will implicitly corrupt the SQLite hashing indexes identically natively. 

Always utilize the secure generic uninstallation parameters explicitly natively.

```bash
euxis agent remove custom_auditor
```
This safely tears down explicit generic route associations securely, wiping the generic cryptographic signatures explicitly seamlessly.
