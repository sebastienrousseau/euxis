# (c) 2026 Euxis Fleet. All rights reserved.
# ADR-005: Specialist Agents for Domain-Specific Expertise

## Status
Accepted

## Context
The fleet's 31 generalist agents handle broad engineering tasks well, but four domains require deep, non-transferable expertise that generalist agents consistently underperform on:

1. **Cryptography** — Constant-time discipline, nonce uniqueness, key zeroization, and post-quantum readiness require specialized knowledge that security generalists lack.
2. **Payments** — ISO 20022 schema compliance, C14N canonicalization, and regulatory requirements (PCI-DSS, PSD2) are highly domain-specific.
3. **Realtime Audio** — Buffer underrun prevention, SCHED_FIFO priority, zero-allocation constraints in audio callbacks, and platform-specific device APIs (ALSA/PipeWire/CoreAudio/WASAPI) demand specialized prompts.
4. **Rust Ecosystem** — MSRV policy, feature flag hygiene (additive-only), `no_std` correctness, `cargo-semver-checks`, and docs.rs quality gates require Rust-specific knowledge.

## Decision
Add four specialist agents with dedicated prompts, intelligence tiering, and organizational infrastructure:

- `cryptographer` — A-Tier (claude) for deep reasoning on cryptographic correctness
- `ledger` — A-Tier (claude) for ISO 20022 domain expertise
- `conduit` — B-Tier (goose) for systems-level audio work
- `custodian` — B-Tier (goose) for Rust toolchain integration

Create a "Specialist" squad to organize these agents, domain-specific playbooks for phased workflows, and two new combos (Seal, Settle) for chained execution.

## Alternatives Considered
1. **Extend existing agents** — Add crypto/payments/audio/rust knowledge to pentester, optimizer, etc. Rejected: violates single-responsibility, dilutes prompt quality.
2. **On-demand prompt injection** — Load domain knowledge via protocol files only when needed. Rejected: insufficient depth; domain agents need persistent mandate and calibrated output format.
3. **External tool integration** — Delegate to cargo-semver-checks, valgrind, etc. directly. Rejected: tools supplement but don't replace domain reasoning about *why* and *when* to apply checks.

## Consequences
- Fleet grows from 31 to 35 agents (4.4% increase in registry size)
- 4 new playbooks and 2 new combos expand the workflow library
- Intelligence tiering adds 2 new sub-tiers (A-Tier: Domain, B-Tier: Systems)
- Specialist squad enables squad-level deployment for domain audits
- Token budget unchanged — specialist prompts load only when invoked
