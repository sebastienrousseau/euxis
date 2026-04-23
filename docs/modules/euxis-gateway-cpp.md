# euxis-gateway-cpp

C++23 HTTP control plane for the Euxis agent framework.

## Overview

HTTP-only gateway server using cpp-httplib. Provides health checks, session CRUD, webhook ingestion, and admin endpoints. Includes HMAC-SHA256 authentication via libsodium and JSONL-based session/audit persistence.

## Key Types

- `GatewayServer` -- HTTP server wrapping `httplib::Server`
- `GatewayConfig` -- Server configuration with timeout presets
- `AuthError` -- Authentication failure type

## Key Functions

- `verify_hmac()` -- HMAC-SHA256 signature verification with constant-time compare
- `verify_bearer_token()` -- Bearer token validation
- State functions: `persist_message()`, `load_session_from_disk()`, `audit_log()`
- Route groups: health, sessions, webhooks, admin

## Dependencies

- `euxis-security-cpp`, `euxis-core-cpp`, `euxis-adapters-cpp`, `euxis-metrics-cpp`
- `httplib::httplib`, `unofficial-sodium::sodium`
- `nlohmann_json::nlohmann_json`, `spdlog::spdlog`

## Build

Part of the euxis-cpp CMake project. Built via `euxis_add_library()` macro.
