# Rust Playbook

The Euxis Rust playbook utilizes the strictness of the Rust compiler and its native ecosystem. Use this playbook to engineer memory-safe, zero-cost abstractions and highly performant libraries.

## Toolchain Integration

The playbook integrates directly into the standard Rust toolchain.

* **Cargo:** Manage dependencies and the entire build subsystem.
* **Clippy:** Enforce strict linting rules and eliminate common architectural mistakes.
* **Rustfmt:** Format code instantly to community standards.

## Execute the Playbook

Initiate a Greenfield project or execute profound architectural refactoring.

```bash
euxis playbook run rust "Implement the background job processor utilizing Tokio."
```

Euxis guarantees that all code compiles natively, passes `cargo test`, and flags zero `clippy` warnings before terminating the execution loop.
