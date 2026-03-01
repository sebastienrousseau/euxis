# Euxis-Superior Architecture Blueprint (2026)

## Objective

Build a security-first, local-first, OpenClaw-compatible agent platform that is faster, safer, and more governable across macOS, Linux, and WSL2.

## 1. Package Architecture

### 1.1 Control and Runtime Packages

- `euxis-core`: Agent orchestration, task graph, retries, bounded execution primitives.
- `euxis-runtime`: Runtime state, memory snapshots, performance/event manifests.
- `euxis-gateway`: External ingress/egress, protocol translation, session routing.
- `euxis-etx` (C++23/Qt6): Desktop GUI with 17 screens, 3 themes, and command palette.
- `euxis-bridge-cpp` (C++23): Skill import, static analysis, admission pipeline, sandbox execution.

### 1.2 Security and Trust Packages

- `euxis-crypto-cpp` (C++23): AES-256-GCM, Ed25519, Argon2id key derivation via libsodium.
- `euxis-security`: Policy engine, command allowlists, verification and enforcement.
- `euxis-policy`: Policy bundles and policy distribution artifacts.

### 1.3 Ecosystem and Delivery Packages

- `euxis-adapters`: Messaging and platform adapters (Telegram/Signal/Slack).
- `euxis-web`: TypeScript/browser-facing crypto and web integration modules.
- `euxis-cli`: Unified command surface (`euxis verify --all`, bridge commands).
- `euxis-metrics`: SLOs, perf/cost telemetry, regression detection.
- `euxis-ops`: Release governance, CI checks, templates, benchmark pipelines.
- `euxis-docs`: Technical specs, ops runbooks, module documentation.

### 1.4 Enforced Internal Layout Pattern

Each Python package follows:

- `src/<pkg>/core/`: platform-agnostic logic only.
- `src/<pkg>/platform/`: OS-specific adapters (macOS/Linux/WSL).
- `tests/unit/`: deterministic, no network.
- `tests/platform/`: adapter behavior and path semantics.

This split ensures portability and predictable maintenance.

## 2. Protocol Stack

### 2.1 Primary Runtime Protocols

- `EUXIS-A2A`: Agent-to-agent envelopes for task delegation and result contracts.
- `EUXIS-KYA`: Know-Your-Agent identity claims, key references, trust level.
- `EUXIS-MEM`: Memory write/read protocol with provenance and integrity fields.
- `EUXIS-EXEC`: Signed execution requests for shell/Node tooling.

### 2.2 OpenClaw Compatibility Protocols

- `OPENCLAW-SKILL-IMPORT`: Parse `SKILL.md`/`openclaw.json` to normalized bridge records.
- `OPENCLAW-SKILL-WRAP`: Wrap JS skills into policy-bound `BridgedSkill` callables.
- `OPENCLAW-MIGRATE`: Import identity/session/transcript data with checksums.

### 2.3 Event Contracts

All protocol messages include:

- `event_id`, `event_type`, `ts_utc`
- `agent_id`, `identity_ref`, `policy_ref`
- `payload`, `integrity_hash`, `signature_ref` (when required)

## 3. Control Plane

### 3.1 Plane Components

- `Identity Service`: issues and rotates agent credentials.
- `Policy Decision Point (PDP)`: evaluates `allow/deny/challenge` decisions.
- `Policy Enforcement Point (PEP)`: enforces decisions in runtime/gateway/bridge.
- `Scheduler`: heartbeat and cron orchestration for autonomous workflows.
- `Audit Pipeline`: append-only JSONL plus hash-chain validation.

### 3.2 Control Flow

1. Agent proposes an action (`EUXIS-EXEC` or tool call).
2. PDP checks identity, policy, risk score, and requested capability.
3. PEP executes via bounded adapter if approved.
4. Result and attestations are persisted to audit + memory channels.
5. Scheduler can replay or resume long-horizon tasks from signed checkpoints.

## 4. Security Layers (Defense in Depth)

### Layer 0: Supply Chain

- Signed artifacts and dependency pinning.
- Locked build/test profiles for reproducibility.

### Layer 1: Identity

- KYA credentials for each agent, bridge daemon, and automation actor.
- Explicit capability scopes (`read`, `write`, `exec`, `network`).

### Layer 2: Execution Guardrails

- Allowlist-only shell and Node command execution.
- Bounded timeouts, bounded output, controlled environment variables.
- Explicit deny for unmapped OpenClaw skills.

### Layer 3: Data Security

- AES-256 encrypted state and secrets at rest.
- Signed memory mutations for sensitive workflows.
- Import provenance manifest for migration operations.

### Layer 4: Observability and Response

- Structured security events with tamper-evident logs.
- Incident-friendly replay bundles and strict forensic traces.

## 5. Performance and Cost Architecture

- Local-first inference path for low latency and low cost.
- Optional cloud escalation for high-complexity tasks by policy.
- Sparse-attention local model fallback for routine coding/ops tasks.
- Caching of deterministic tool outputs and normalized memory snapshots.
- Benchmark gates in CI (`latency`, `RSS`, `CPU`, `regression threshold`).

## 6. Euxis-Bridge Implementation Contract

`euxis-bridge-cpp` must provide:

1. `ClawHubImporter` to parse/import OpenClaw skills.
2. `NodeSkillBridge` to run JavaScript handlers with strict bounds.
3. `openclaw_skill` decorator to register wrapped skills in Python runtime.
4. Bridge CLI for import and registry export.
5. Deterministic tests for parser, importer, and command execution policy.

## 7. Rollout Milestones

### Milestone 1 (Now)

- Deliver `EUXIS_V2_ARCH.md`.
- Ship runnable `euxis-bridge-cpp` scaffolding.
- Add tests and docs for import + wrapper flows.

### Milestone 2

- Integrate KYA signing/verification into bridge execution.
- Wire bridge package into `verify-all-packages` policy gates.

### Milestone 3

- Messaging-bridge daemon sub-agents (Telegram/Signal).
- Heartbeat scheduler for autonomous daily operations.

### Milestone 4

- Hierarchical memory graph with provenance and integrity checks.
- OSWorld-style agent harness for long-horizon reliability benchmarking.

### Milestone 5

- Production hardening: stricter policy bundles, release evidence, split-repo readiness.

## 8. Why Python Core + Node Tooling Wins in 2026

- Python gives stronger orchestration, security policy, evaluation, and enterprise governance.
- Node remains optimal for existing JS skill ecosystems and browser-heavy toolchains.
- Separation of concerns lowers breach blast radius and keeps interoperability high.
- This architecture preserves OpenClaw compatibility while maintaining stricter trust boundaries.

