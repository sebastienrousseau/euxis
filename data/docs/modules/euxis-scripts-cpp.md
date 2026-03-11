# euxis-scripts-cpp

C++23 quality enforcement and documentation integrity checks.

## Overview

Enforces technical debt baselines (pragma/TODO limits) and validates documentation structure (required docs, module pages).

## Key Types

- `DebtReport` -- Pragma and TODO counts with pass/fail status

## Key Functions

- `check_tech_debt(repo_root)` -- Scan source directories for debt markers
- `docs_source_files(repo_root)` -- Find all doc files under docs/
- `missing_required_docs(repo_root)` -- Check for mandatory documentation
- `build_docs_quality_report(repo_root, packages)` -- Comprehensive quality report

## Dependencies

- `nlohmann_json::nlohmann_json`

## Build

Part of the euxis-cpp CMake project. Built via `euxis_add_library()` macro.
