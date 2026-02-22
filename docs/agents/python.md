# Python Agents & Playbooks

The Euxis Python playbook operates recursively on your Python codebase, ensuring it meets the highest standards for modern Python development.

## Supported Tools

The Python playbook is configured to enforce and utilize:
*   **Ruff**: Extremely fast Python linter and formatter.
*   **uv**: Ultra-fast Python package and project manager.
*   **Pytest/Hypothesis**: For comprehensive unit testing and property-based testing.
*   **MyPy/Pyright**: For strict static type checking.

## Usage

To run the Python modernization playbook:

```bash
euxis playbook run python "Refactor the authentication module"
```

This will trigger a multi-phase execution involving the `researcher`, `architect`, `maintainer`, `tester`, and `reviewer` agents configured specifically for Python codebases.
