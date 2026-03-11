# euxis-bridge-cpp

C++23 bridge hardening — skill import, static analysis, admission pipeline, sandbox execution, provenance tracking.

## Overview

euxis-bridge-cpp secures the OpenClaw/ClawHub skill import pipeline. It parses SKILL.md frontmatter, runs a 19-pattern static analyzer against skill code, enforces a 3-stage admission pipeline (parse, verify, analyze), verifies Ed25519 signatures, builds SLSA v1.0 provenance chains, and executes admitted skills inside nsjail (Linux) or sandbox-exec (macOS) process isolation. An append-only JSONL audit log records every import decision alongside author reputation scores.

## Dependencies

- euxis-crypto-cpp
- libsodium
- nlohmann-json
- spdlog

## Building

```bash
# From the euxis-cpp root
cmake -B build -S .
cmake --build build --target euxis-bridge-cpp
```

## Testing

```bash
ctest --test-dir build -R euxis-bridge-cpp_tests
```

## API

- **skill.hpp** -- Skill data model (name, version, author, permissions, code).
- **policy.hpp** -- Admission policy definitions and default rule sets.
- **importer.hpp** -- High-level skill import orchestrator.
- **parser.hpp** -- SKILL.md YAML frontmatter and body parser.
- **verification.hpp** -- Ed25519 signature verification for skill packages.
- **static_analysis.hpp** -- 19-pattern static analyzer for dangerous code constructs.
- **admission.hpp** -- 3-stage admission pipeline (parse, verify, analyze).
- **reputation.hpp** -- Author reputation scoring and trust level computation.
- **provenance.hpp** -- SLSA v1.0 provenance chain construction and validation.
- **audit.hpp** -- Append-only JSONL audit log writer and reader.
- **executor.hpp** -- Sandboxed skill execution (nsjail/sandbox-exec).
- **platform.hpp** -- Platform detection and sandbox backend selection.
