# Euxis Identity C++

The `euxis::identity` module establishes the strict cryptographic identity layer for the Euxis Agent OS. It implements Non-Human Identity (NHI) governance, Verifiable Credentials, and ERC-8004 agent cards.

## Cryptographic Provenance

Every agent interacting within the OS must possess a provable, mathematically sound identity.

* **Precondition**: An agent must present a valid `AgentCard` signed by the root authority.
* **Postcondition**: Grants the agent an ephemeral authorization token for execution.

Use the `W3C DID` specification to model agent identities. The system enforces Ed25519 signature checks on all incoming requests to prevent impersonation or man-in-the-middle escalation.

## Agent Card Validation

The `ERC-8004` standard defines the core metadata for autonomous entities.

* **Type Erasure**: Hiding concrete implementations — Decouple identity structures from physical transports.
* **std::span**: Bounds-checked memory view — Safe memory buffer operations.

When an agent requests entry to a swarm, execute the `verify_attestation` method. The module relies on C++23 `std::expected` to propagate parsing or cryptographic failures deterministically.

```cpp
auto valid = registry.verify_attestation(card_payload)
    .and_then([](auto&& card) { return check_revocation(card); })
    .or_else([](auto&& err) { return reject_agent(err); });
```

## NHI Governance

Agents are inherently untrusted. The identity module maps directly into the `libs/platform` eBPF/KVM sandboxing logic.

* **UB**: Undefined Behavior — Issuing a capability token without a validated DID.

Always scope the agent's memory and networking access to the exact bounds defined within its verifiable credential. Do not grant implicit permissions.