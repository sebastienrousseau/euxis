# Naming Conventions

This policy defines how we name folders and files so the repo stays predictable and easy to navigate.

## Scope

- Enforcement focus (current): docs and support files (policies, guides, config, scripts, templates).
- Code and package sources may follow their language/tooling conventions.
- Applies to all new or renamed folders/files within the enforced scope.
- Existing legacy names are allowed until they are modified.
- Full-repo rename passes require explicit confirmation before starting.

## Folder Naming

- Use `kebab-case` for all folder names.
- Examples: `user-authentication`, `payment-processing`, `agent-runtime`.
- Allowed exceptions:
  - Hidden folders (e.g., `.github`, `.git`).
  - Vendor or generated folders managed by tools.
  - Data snapshots where the naming encodes upstream identifiers.

## File Naming

- Use `camelCase` for file names.
- Examples: `userProfile.js`, `paymentService.ts`, `auditReport.md`.
- Allowed exceptions:
  - `README.md` (required per folder).
  - Root-level high-level files: `README.md`, `LICENSE`, `CHANGELOG.md`, `.gitignore`, `CONTRIBUTING.md`, `SECURITY.md`, `CODE_OF_CONDUCT.md`.
  - Tooling-required filenames (e.g., `pyproject.toml`, `mkdocs.yml`).
  - Language-specific conventions (e.g., `__init__.py`, `go.mod`, `Cargo.toml`, `package.json`).

## Avoid Ambiguous Suffixes

Do not use `final`, `latest`, or `updated` in filenames. Prefer versions or dates:

- Versioned: `releaseNotes-v1.2.0.md`
- Timestamped: `releaseNotes-current-02-16.md`

## README.md Required

Every folder must include a `README.md` explaining:

- Purpose of the folder.
- Setup steps (if any).
- Usage (commands or entry points).
- Dependencies (internal or external).

### Minimal README Template

```md
# Folder Purpose

## Purpose

## Setup

## Usage

## Dependencies
```

## Automation

Use the naming checker to validate changes (new, renamed, or modified files/folders):

```bash
python scripts/check-naming-conventions.py --changed
```

For full-repo audits:

```bash
python scripts/check-naming-conventions.py --all
```

## Shared Templates

- Use `.editorconfig` as the default formatting baseline.
- Start new Node packages from `config/templates/package.json`.

## Exceptions

If a toolchain or language standard requires a different naming pattern, add a scoped exception in `config/namingExceptions.txt`.
If a legacy file must remain unchanged, add a scoped exception with a short rationale and revisit during a future cleanup.
