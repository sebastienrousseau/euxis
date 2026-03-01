# euxis-runtime-cpp

C++23 runtime package structure validator.

## Overview

Validates runtime data layout: required files, JSONL format integrity, performance metrics schema, lifecycle state files, and JSON manifests.

## Key Types

- `RuntimeValidator` -- Validates runtime package structure
- `RuntimeValidationError` -- Exception for validation failures

## Key Functions

- `validate_runtime_layout(root)` -- Convenience entry point for full validation

## Dependencies

- `nlohmann_json::nlohmann_json`

## Build

Part of the euxis-cpp CMake project. Built via `euxis_add_library()` macro.
