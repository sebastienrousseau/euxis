# Go Playbook

The Euxis Go playbook enforces strict Go idioms, standard error handling patterns, and memory-safe concurrency models. Use this playbook to scaffold high-throughput microservices or CLI utilities.

## Toolchain Integration

Euxis integrates flawlessly with the standard Go ecosystem. It does not introduce proprietary configurations.

* **Go Modules (`go mod`):** Native dependency resolution and vendoring.
* **Testing (`go test`):** Automated benchmark generation and unit test validation.
* **Formatting (`gofmt` / `goimports`):** Guaranteed syntactical conformity.
* **Static Analysis (`golangci-lint`):** Aggressive, multi-pass linting to prevent routine errors.

## Execute the Playbook

Trigger system-level refactoring or architectural scaffolding operations.

```bash
euxis playbook run go "Build a highly concurrent worker pool utilizing channels."
```

If `golangci-lint` fails, Euxis repairs the logic autonomously before returning execution control.
