# Changelog & Releases

This document tracks all significant version updates, feature additions, and breaking changes to the Euxis Mesh Gateway and CLI.

We adhere strictly to Semantic Versioning (`MAJOR.MINOR.PATCH`).

---

## [v0.0.2] - The Core API Evolution

*Currently in active development via `feat/v0.0.2`.*

### Added
* **Plugin Architecture:** Official support for compiling and registering custom Rust Extism WebAssembly modules (`euxis agent build`).
* **Cryptographic Signatures:** ECDSA verification enforced on all Wasm execution targets.
* **Liquid Glass TUI:** Entirely rewritten `euxis ui` dashboard featuring real-time telemetry streaming over WebSockets natively.

### Changed
* **Documentation Overhaul:** Restructured the `docs/` repository to conform to the 2026 User-Centric Design journey framework.

---

## [v0.0.2] - The Genesis Mesh

*Initial Release*

### Added
* `euxis-core` Python engine integration for executing basic Extism payloads.
* Basic CLI positional routing (`euxis <agent> [intent]`).
* `architect`, `maintainer`, and `tester` native Wasm agents natively bundled.
* Initial WASI filesystem constraint mappings.
