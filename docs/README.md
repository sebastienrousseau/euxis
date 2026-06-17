# Euxis Documentation

This directory holds every public-facing doc for the project: architecture overviews, agent and library guides, ADRs, API references, security policy, and the AIO writing mandate.

## Where to start

If you are reading the code for the first time, start with `architecture.md` for the system-level picture and `lib-architecture.md` for the module breakdown. **Both documents currently describe the legacy shell prototype**; a C++23 rewrite is in progress and the libraries under `libs/` are authoritative until those docs catch up.

If you are extending Euxis, read `guides/agent-development-guide.md` for the agent-side workflow and the example tree under `examples/cpp/` for the SDK-side patterns. The `tool_calling_loop/` example demonstrates the closed agent loop with approval gating, and `streaming_loop/` demonstrates the coroutine-based provider API.

If you are writing docs of your own, read `AIO-mandate.md` first. The mandate is a three-rule writing convention that every new doc in this tree is expected to follow.

## Layout

| Subdirectory  | What lives here |
|---------------|-----------------|
| `guides/`     | How-to guides for agents, fleet operations, integrations |
| `reference/`  | API references and command tables |
| `adr/`        | Architecture Decision Records |
| `examples/`   | C++ SDK examples (one subdir per example) |
| `benchmarks/` | Benchmark methodology, results, and trend notes |
| `audits/`     | Past security and architecture audits |
| `archive/`    | Superseded or historical documents kept for traceability |

## Tooling

Documentation is consumed by MkDocs and rendered to the public site from `mkdocs.yml`. Doxygen generates the C++ API reference under `EUXIS_BUILD_DOCS=ON` (default in CMake; auto-skips if Doxygen is missing).
