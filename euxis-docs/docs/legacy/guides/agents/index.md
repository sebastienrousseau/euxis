# Euxis Agents

Agents are the atomic units of execution within the Euxis autonomous mesh. Operating as deterministic, cryptographically signed WebAssembly (Wasm) modules, they execute within a strict, hyper-low latency Extism sandbox.

Use this documentation to construct, orchestrate, and deploy Euxis Agents across your infrastructure.

## Ecosystem Architecture

Euxis categorizes agents into functional hierarchies to manage complex intent resolution.

### Core Orchestrators
Use Core Agents to route and validate high-level directives.
* **Architect**: Compile system designs and operational manifests.
* **Orchestrator**: Coordinate multi-agent communication structures.
* **Auditor**: Verify compliance and structural integrity.
* **Guard**: Isolate threats and validate capability bounds.

### Fleet Specialists
Deploy Fleet Agents to execute domain-specific workloads.
* **Development**: Debug, optimize, and maintain execution code.
* **Security**: Pen-test and harden exposed execution surfaces.
* **Creative**: Generate structured copy and responsive interface designs.

## Playbook Integration

Euxis executes localized language models—called Playbooks—to enforce native ecosystem standards without prompting overhead. Euxis ensures 100% coverage across the following runtimes:

* [Rust Playbook](rust.md)
* [Python Playbook](python.md)
* [Go Playbook](go.md)
* [JavaScript & TypeScript Playbook](javascript.md)
* [Swift Playbook](swift.md)
* [CSS & HTML Playbook](css.md)

## Deployment Primitives

Orchestrate your agent mesh directly from the command line. Euxis provisions dedicated sub-agents dynamically based on your directive context.

### Single Invocation
Trigger an isolated analysis sweep via the core Architect.

```bash
euxis architect "Design a granular RBAC policy for user authentication."
```

### Squad Deployment
Federate a dedicated task force to resolve systemic architecture refactoring.

```bash
euxis squad deploy quality "Audit and refactor the entire payment processing cluster."
```

### Playbook Execution
Enforce strict compiler and linting guidelines automatically within a native context.

```bash
euxis playbook run rust "Implement standard Axum server routing."
```
