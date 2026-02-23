# Command Line Interface (CLI)

The `euxis-cli` application provides the primary entrypoint for single-shot agent execution, mesh debugging, and configuration management. It is structurally designed to handle rapid, contextual intent parsing.

## Basic Syntax

The universal command syntax relies on positional delegation. You specify the target agent as the first positional argument, followed immediately by your natural language intent.

```bash
euxis <agent> [intent] [options]
```

## Exploring Capabilities

Use the `list` command to explore available local intelligence nodes in your registry.

```bash
euxis list
```

To display specific agent parameters and their required WebAssembly memory imports:

```bash
euxis inspect architect
```

## Global Execution Flags

Append explicit system flags to mutate identical execution routes.

| Flag | Purpose | Warning |
|---|---|---|
| `--artifact-only` | Suppress all conversational telemetry. Yield strictly the generated code artifact or verification boolean. | Negates TUI observability. Use exclusively for CI pipelines. |
| `--sandbox off` | Disable the Extism memory container completely and map directly to the host system call routes. | Absolute catastrophic risk. Never execute remote un-signed agents using this parameter. |
| `--cwd <path>` | Shift the root execution environment without navigating the native shell structure. | None. |

## Generating Manifests

Agents execute natively as `wasm32-wasi` modules explicitly bound to operational constraints. Produce secure, isolated execution capability models utilizing the `manifest` builder.

```bash
euxis manifest generate rust_compiler > rust_manifest.json
```

Use manuals within the [Security Integration](../security/manifests.md) documentation to securely limit virtual directory overrides within your customized JSON schemas.
