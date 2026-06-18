<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">apps/cli — <code>euxis</code></h1>

<p align="center">
  The main euxis command-line binary: code scans, SBOM emission,
  signed evidence bundles, agent orchestration, and a terminal UI for
  monitoring fleet activity.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start)
- [Commands](#commands)
- [Public surface](#public-surface) — for embedders
- [Exit codes](#exit-codes)
- [License](#license)

---

## Install

```bash
cmake -B build/cmake-build
cmake --build build/cmake-build --target euxis-cli
ln -s "$(pwd)/build/cmake-build/apps/cli/euxis-cli" ~/bin/euxis
```

The binary depends on the full `libs/` graph; no external runtime dependencies beyond what CMake links in.

## Quick start

```bash
# Verify a repository (standard mode)
euxis check .

# Fast bounded triage (flash mode)
euxis triage .

# Deep verification with cross-checking
euxis review .

# Triage vs deep diff — what flash missed
euxis compare .

# Recent validation metrics + confidence drift
euxis stats --last 5

# Environment diagnostics — toolchain, providers, credentials, cache
euxis doctor
```

## Commands

| Command | What it does |
|---|---|
| `euxis check [target]`    | Standard verification — full audit pipeline |
| `euxis triage [target]`   | Bounded flash scan — fast, lower-coverage, pre-commit-friendly |
| `euxis review [target]`   | Deep verification — slowest, highest confidence |
| `euxis compare <target>`  | Runs both triage and standard; reports the delta |
| `euxis stats`             | Metrics + confidence-drift history |
| `euxis policy <sub>`      | Inspect and enforce policy rules |
| `euxis doctor`            | Probe toolchain, provider creds, cache health |
| `euxis playbook <name>`   | Run a named playbook (multi-agent script) |
| `euxis <agent-id> "..."`  | Invoke a single named agent from the fleet |

A full flag reference lives at `docs/reference/cli-reference.md`.

## Public surface (for embedders)

Headers exposed under `apps/cli/include/euxis/cli/` so other binaries can embed the engine:

| Header | What it owns |
|---|---|
| `engine.hpp` | `Engine` — the command-dispatch core |
| `session.hpp` | `Session` — per-invocation state |
| `command.hpp` | Base command type + registration |
| `config_loader.hpp` | Loads `data/config/*.json` with caching via `libs/cache` |
| `lang_detect.hpp` | `LanguageProfile`, `DetectedLanguage`, `DetectedFramework` — heuristics for project-type detection |
| `mcp_provider.hpp` | `McpProviderBridge`, `McpServerConfig` — talks to external MCP servers |
| `lsp_bridge.hpp` | `LspDiagnostic`, `LspSeverity` — surfaces findings to LSP clients |
| `terminal.hpp` | `TerminalScreen`, `TableRow`, `Cell` — TUI rendering |
| `term_caps.hpp` | `TermCaps` — runtime detection of truecolour / unicode / size |
| `exit_codes.hpp` | `ExitCode` enum |
| `sarif.hpp` | `SarifFinding` — SARIF 2.1.0 emission |
| `box.hpp`, `autofix.hpp`, `certification.hpp`, ... | Per-feature surfaces |

## Exit codes

`ExitCode` (`apps/cli/include/euxis/cli/exit_codes.hpp`) defines the canonical exit-code set so CI scripts can branch deterministically:

| Code | Meaning |
|---|---|
| 0  | No findings (or all findings dismissed by policy) |
| 1  | Findings above the configured severity floor |
| 2  | Tool / environment error (missing dependency, malformed config) |
| 3  | Authentication / credential failure |
| 4  | Internal error (assertion or panic) |

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
