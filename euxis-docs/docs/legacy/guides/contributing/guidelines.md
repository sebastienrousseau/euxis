# Contributing Guidelines

We welcome contributions to the Euxis autonomous mesh! Whether you are fixing a bug, expanding native Playbooks, or writing custom Extism WebAssembly modules, your efforts are appreciated.

## 🤝 Code of Conduct

All contributors and maintainers are expected to abide by our standard Code of Conduct. Please be respectful, constructive, and inclusive in all GitHub discussions and Pull Request reviews.

## 🐛 Reporting Bugs

If you trigger a `SandboxViolationError`, discover a memory leak, or encounter a Gateway execution trap:

1. **Verify the Issue:** Ensure you are running the latest `euxis-core` Engine and CLI.
2. **Search Existing Issues:** Check the GitHub issue tracker to prevent duplicates.
3. **Generate a Telemetry Dump:** Run `euxis support generate` to securely package your execution state (keys are natively scrubbed).
4. **Submit:** Open a new Bug Report on GitHub and attach the `.tar.gz` archive.

## ✨ Proposing Features

Feature requests should begin with a GitHub Discussion or Issue. 
* Detail **why** the feature is necessary (e.g., "Add generic Zig Playbook support for native Wasm compilation").
* Describe the expected Extism boundaries and WASI capability constraints.

## 🛠️ Pull Request Process

1. **Fork the Repository:** Create your own feature branch (`feat/your-name-zig-support`).
2. **Follow Strict Formatting:**
   - **Rust:** Run `cargo fmt` and `cargo clippy` before committing.
   - **Python:** Format all core integrations with `ruff check --fix` and `ruff format`.
3. **Write Tests:** New native extensions MUST include `pytest` or `cargo test` suites maintaining 100% execution coverage.
4. **Open the PR:** Describe the structural changes thoroughly. The Euxis CI/CD `Auditor` agent will automatically review your Pull Request natively.

## 📖 Documentation Contributions

Documentation is treated exactly like code. All `.md` files must adhere to the `kebab-case.md` naming convention and reside properly formatted inside the `docs/guides/` tree.
