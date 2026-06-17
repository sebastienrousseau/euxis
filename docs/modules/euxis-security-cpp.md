# euxis-security-cpp

C++23 security policy module for gateway-facing packages.

## Overview

Provides portable baseline security policy defaults: auth mode validation, HTTPS requirements, MFA enforcement, and query token controls.

## Key Types

- `SecurityPolicy` -- Immutable policy struct with JSON serialization
- `PolicyError` -- Exception for validation failures

## Key Functions

- `validate_auth_mode(mode)` -- Normalize and validate auth modes (token/password/none)
- `merge_policy(overrides)` -- Create policy from defaults plus optional JSON overrides

## Dependencies

- `nlohmann_json::nlohmann_json`

## Build

Part of the euxis-cpp CMake project. Built via `euxis_add_library()` macro.
