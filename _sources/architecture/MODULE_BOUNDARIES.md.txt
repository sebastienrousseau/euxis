# Module Boundaries

This repository is a modular monolith. Each top-level domain directory is a
candidate for extraction into its own repository. Modules should only depend on
public APIs and shared utilities under `packages/shared`.

## Domains
- `adapters`: External integrations (Slack, Telegram, etc.)
- `api`: Gateway and API surface (FastAPI, webchat assets)
- `cli`: CLI entry points and orchestration commands
- `core`: Shared core logic and shell libraries
- `crypto`: Cryptography services and key management
- `metrics`: Evidence, verification, and performance metrics
- `ui`: TUI/UX application
- `config`: Configuration schemas and helpers
- `security`: Security-related tooling and docs
- `packages/shared`: Cross-domain utilities and shared helpers

## Dependency Rules
- Domains may depend on `packages/shared`.
- Domains should not import internal paths from other domains.
- All inter-module dependencies must go through public APIs or shared packages.

## Extraction Order (Suggested)
1. `packages/shared`
2. `crypto`
3. `metrics`
4. `adapters`
5. `api`
6. `ui`
7. `cli`
8. `core`
9. `config`
10. `security`
