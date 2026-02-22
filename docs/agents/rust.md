# Rust Agents & Playbooks

The Euxis Rust playbook utilizes the strictness of the Rust compiler and its ecosystem to produce memory-safe and highly performant applications.

## Supported Tools

The playbook is integrated with the standard Rust toolchain:
*   **Cargo**: Dependency manager and build system.
*   **Clippy**: For linting and finding common mistakes.
*   **Rustfmt**: For ensuring standard formatting.

## Usage

To kick off a Rust project or refactor an existing one:

```bash
euxis playbook run rust "Implement the background job processor"
```

The Euxis agents will ensure all code compiles cleanly, passes `cargo test`, and throws zero `clippy` warnings before completing the task.
