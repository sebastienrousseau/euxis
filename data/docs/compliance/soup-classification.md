# SOUP Classification Register

> Software of Unknown Provenance — IEC 62304 / ISO 13485 Annex A

**Document ID:** EUXIS-SOUP-001
**Version:** 1.0
**Date:** 2026-03-18
**Classification Standard:** IEC 62304:2006+A1:2015, ISO 13485:2016

## Purpose

This register identifies and classifies all third-party software components (SOUP) used in the Euxis project. Each component is assessed for risk level, license compatibility, and validation status per IEC 62304 §8.1.2.

## Risk Classification

| Level | Description | Validation Required |
|-------|-------------|---------------------|
| **A** | No contribution to hazardous situation | Identification only |
| **B** | Could contribute to hazardous situation, not Class C | Published anomaly list review |
| **C** | Could contribute to death or serious injury | Full verification & validation |

All Euxis SOUP is classified **Level A** — the software is a developer tooling CLI, not a medical device or safety-critical system. Classifications are maintained for process compliance.

## SOUP Register

### 1. nlohmann/json

| Field | Value |
|-------|-------|
| **Name** | nlohmann JSON for Modern C++ |
| **Version** | v3.11.3 |
| **Source** | https://github.com/nlohmann/json |
| **License** | MIT |
| **Risk Class** | A |
| **Purpose** | JSON parsing/serialization for config, artifacts, agent output |
| **Integration** | FetchContent (CMake) |
| **Known Anomalies** | None affecting Euxis usage |
| **Validation** | Unit tests cover all JSON paths; CI builds verify linkage |

### 2. pantor/inja

| Field | Value |
|-------|-------|
| **Name** | Inja — Jinja2 for C++ |
| **Version** | v3.4.0 |
| **Source** | https://github.com/pantor/inja |
| **License** | MIT |
| **Risk Class** | A |
| **Purpose** | Template rendering for agent prompts and reports |
| **Integration** | FetchContent (CMake) |
| **Known Anomalies** | None affecting Euxis usage |
| **Validation** | Template rendering covered by agent execution tests |

### 3. jbeder/yaml-cpp

| Field | Value |
|-------|-------|
| **Name** | yaml-cpp |
| **Version** | 0.8.0 |
| **Source** | https://github.com/jbeder/yaml-cpp |
| **License** | MIT |
| **Risk Class** | A |
| **Purpose** | YAML configuration file parsing |
| **Integration** | FetchContent (CMake) |
| **Known Anomalies** | Requires CMAKE_POLICY_VERSION_MINIMUM=3.5 for compat |
| **Validation** | Config loading tests verify parse correctness |

### 4. gabime/spdlog

| Field | Value |
|-------|-------|
| **Name** | spdlog |
| **Version** | v1.15.0 |
| **Source** | https://github.com/gabime/spdlog |
| **License** | MIT |
| **Risk Class** | A |
| **Purpose** | Structured logging (audit trail, diagnostics) |
| **Integration** | FetchContent (CMake) |
| **Known Anomalies** | None affecting Euxis usage |
| **Validation** | Log output verified in integration tests |

### 5. google/googletest

| Field | Value |
|-------|-------|
| **Name** | Google Test |
| **Version** | v1.14.0 |
| **Source** | https://github.com/google/googletest |
| **License** | BSD-3-Clause |
| **Risk Class** | A (test-only, not shipped) |
| **Purpose** | Unit and integration testing framework |
| **Integration** | FetchContent (CMake) |
| **Known Anomalies** | None |
| **Validation** | N/A — test infrastructure only |

### 6. machinezone/IXWebSocket

| Field | Value |
|-------|-------|
| **Name** | IXWebSocket |
| **Version** | v11.4.4 |
| **Source** | https://github.com/machinezone/IXWebSocket |
| **License** | BSD-3-Clause |
| **Risk Class** | A |
| **Purpose** | WebSocket client for MCP protocol and real-time communication |
| **Integration** | FetchContent (CMake), TLS disabled |
| **Known Anomalies** | Missing `<cstdint>` in some headers (patched upstream) |
| **Validation** | WebSocket transport tests verify connectivity and framing |

### 7. yhirose/cpp-httplib

| Field | Value |
|-------|-------|
| **Name** | cpp-httplib |
| **Version** | v0.18.7 |
| **Source** | https://github.com/yhirose/cpp-httplib |
| **License** | MIT |
| **Risk Class** | A |
| **Purpose** | HTTP client/server for gateway and API communication |
| **Integration** | FetchContent (CMake), OpenSSL not required |
| **Known Anomalies** | None affecting Euxis usage |
| **Validation** | Gateway route tests and API integration tests |

### 8. SQLite3

| Field | Value |
|-------|-------|
| **Name** | SQLite |
| **Version** | System-provided (>= 3.36) |
| **Source** | https://sqlite.org |
| **License** | Public Domain |
| **Risk Class** | A |
| **Purpose** | Local persistent storage for memory tier and sessions |
| **Integration** | System package (`find_package(SQLite3 REQUIRED)`) |
| **Known Anomalies** | None affecting Euxis usage |
| **Validation** | Memory store tests verify CRUD operations |

### 9. libsodium

| Field | Value |
|-------|-------|
| **Name** | libsodium |
| **Version** | System-provided (>= 1.0.18) |
| **Source** | https://libsodium.gitbook.io |
| **License** | ISC |
| **Risk Class** | A |
| **Purpose** | Cryptographic primitives (Ed25519, AES-GCM, key derivation) |
| **Integration** | System package (vcpkg or pkg-config) |
| **Known Anomalies** | None |
| **Validation** | Crypto unit tests (Ed25519 sign/verify, AES-GCM encrypt/decrypt, KDF) |

## License Summary

| License | Components | Compatible |
|---------|-----------|------------|
| MIT | nlohmann_json, inja, yaml-cpp, spdlog, cpp-httplib | Yes |
| BSD-3-Clause | googletest, IXWebSocket | Yes |
| Public Domain | SQLite | Yes |
| ISC | libsodium | Yes |

All licenses are permissive and compatible with proprietary distribution.

## Review Schedule

This register is reviewed:
- On every dependency version bump
- Quarterly as part of the software maintenance plan
- When new dependencies are added

## Approval

| Role | Name | Date |
|------|------|------|
| Author | — | 2026-03-18 |
| Reviewer | — | Pending |
| Approver | — | Pending |
