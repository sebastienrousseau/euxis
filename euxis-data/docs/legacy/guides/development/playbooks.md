# Language Playbooks

Playbooks are predefined combinations of specialized agents tuned specifically for a programming language ecosystem. The Euxis gateway intrinsically understands how to compile, test, and enforce styling for specific environments natively.

## Execution Syntax

Execute a playbook directly against your current working directory. The command parses your natural language directive and delegates it to the language-specific agent squad.

```bash
euxis playbook run <language> "<directive>"
```

### Supported Environments

Euxis guarantees 100% ecosystem integration for the following environments:

1. **`python`**: Enforces strict `ruff` formatting, `uv` packaging, and comprehensive `pytest` assertions natively.
2. **`rust`**: Integrates exclusively with `cargo`, `clippy` linting, and standard WebAssembly compilation constraints.
3. **`go`**: Natively generates `gofmt` compliant modules, channels, and error patterns.
4. **`javascript` / `typescript`**: Scaffolds Node/Bun/Deno applications via `tsc` strict compliance mapping.
5. **`swift`**: Leverages Swift Package Manager (SPM) architecture and `XCTest` natively.
6. **`css`**: Compiles structural styles utilizing TailwindCSS utilities and `stylelint` constraints natively.

## The Playbook Lifecycle

When you trigger a playbook, it launches a pre-configured architecture queue natively via the Gateway:

1. **The Researcher** parses external dependencies and version metrics.
2. **The Architect** scaffolds the component or module boundaries.
3. **The Maintainer** injects the actual logic.
4. **The Tester** iteratively invokes language-specific test runners (`cargo test`, `vitest`) natively. If assertions fail, the feedback natively routes back to the Maintainer to self-heal.
5. **The Guard** guarantees the output syntax conforms natively to your standard linter (`ruff`, `clippy`).

## Overwriting Defaults

Playbooks inherit configuration from standard repository definition files (`pyproject.toml`, `Cargo.toml`). Ensure your configurations are committed before invoking playbooks natively to avoid un-intended stylistic drift.
