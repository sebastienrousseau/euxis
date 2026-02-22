# Go Agents & Playbooks

The Euxis Go playbook embraces standard Go idioms and error handling patterns.

## Supported Tools

The playbook uses standard Go tooling:
*   **Go Modules**: Dependency management.
*   **go test**: Benchmark and unit testing.
*   **gofmt / goimports**: Strict formatting.
*   **golangci-lint**: Aggressive linting rules.

## Usage

For any generic Go refactoring or greenfield projects:

```bash
euxis playbook run go "Build a highly concurrent worker pool"
```
