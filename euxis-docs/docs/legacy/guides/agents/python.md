# Python Playbook

The Euxis Python playbook operates recursively on your codebase. It enforces modern standardizations, hyper-fast linting, and comprehensive typing. 

## Toolchain Integration

The Python playbook deploys multiple agents to enforce these capabilities:

* **Ruff:** Analyze and format code in milliseconds.
* **uv:** Manage Python packages and environments with ultra-fast Rust binaries.
* **Pytest & Hypothesis:** Execute comprehensive unit tests and property-based regression checks.
* **MyPy & Pyright:** Enforce strict static type checking.

## Execute the Playbook

Trigger the Python modernization playbook to restructure or analyze code.

```bash
euxis playbook run python "Refactor the backend authentication module."
```

This command deploys the `researcher`, `architect`, `maintainer`, `tester`, and `reviewer` agents in an orchestrated pipeline customized for Python execution.
