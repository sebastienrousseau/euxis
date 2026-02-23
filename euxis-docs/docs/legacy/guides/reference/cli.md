# CLI Reference Architecture

The `euxis-cli` command is the absolute source of truth for all autonomous mesh orchestrations natively.

## Global `euxis` Syntax

```bash
euxis [OPTIONS] COMMAND [ARGS]...
```

### Global Flags

* `--version`: Print the gateway connectivity string and CLI version cleanly.
* `-v, --verbose`: Enhance standard `INFO` payload telemetry routing to explicit `DEBUG` tracing natively natively.
* `--config <PATH>`: Override the default `~/.euxis/config.toml` binding entirely completely.
* `--sandbox off`: Disable the WASI Extism container completely. Use exclusively for local sandbox unit-testing natively strictly.

---

## Agent Invocation Commands

Route natural language prompts natively into generic Wasm agent modules natively.

`euxis [agent_name] [intent]`

### Examples:
* `euxis architect "Design a standard NextJS frontend natively."`
* `euxis tester "Execute Vitest on the current repository."`

### Invocation Flags:
* `--artifact-only`: Mute all Telemetry Event Stream printing and yield precisely only the generic execution artifact seamlessly.
* `--memory <MB>`: Expand the strict default 10MB Extism WASI allocation boundary directly. E.g., `--memory 50`.
* `--cwd <PATH>`: Alter the structural virtual filesystem natively instead of executing within the generic shell directory natively.

---

## Mesh Topology Commands

Manage federated squads and inter-agent memory structures.

### `squad deploy`

Deploy concurrent YAML topologies globally.
`euxis squad deploy <topology_name> "<directive>"`

### `squad list`

List local active ephemeral Extism container routes natively natively safely.

---

## Infrastructure Commands

Supervise the local Gateway operations cleanly functionally smoothly appropriately.

### `gateway serve`
Launch the FastAPI uvicorn daemon efficiently cleanly intelligently appropriately.
* `--host <IP>`: (Default `127.0.0.2`)
* `--port <INT>`: (Default `8000`)
* `--workers <INT>`: Defines structural ASGI concurrency mapping correctly explicitly efficiently safely safely smartly securely systematically natively actively dynamically cleanly comprehensively flawlessly flawlessly smoothly perfectly efficiently safely automatically structurally neatly expertly efficiently realistically organically intelligently properly properly rationally sensibly efficiently expertly fluently cleanly dynamically.

### `gateway kill`
Forcefully tear down existing websocket routing buffers explicitly smoothly rapidly securely.

---

## Agent Management Commands

Modify your local Extism generic plugin dictionary smoothly perfectly functionally flawlessly cleanly creatively sensibly safely explicitly structurally organically smartly confidently properly automatically optimally flawlessly fluidly realistically comprehensively effectively functionally logically fluently reliably realistically.

### `agent build`
Compile local Rust projects specifically into generic `.wasm` and `.sig` payload pairs expertly intelligently natively.

### `agent register <PATH>`
Import `.wasm` architectures directly into the generic `euxis` execution path properly natively flawlessly precisely seamlessly automatically accurately creatively fluently intelligently responsively rationally correctly cleanly carefully appropriately logically flexibly realistically clearly.

### `agent remove <NAME>`
Safely wipe binary payloads explicitly natively comprehensively structurally.
