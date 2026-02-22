# Euxis v0.0.1 Milestone Plan

## Goal
Ship the first production-grade Gateway release with hardened security, reliable channel onboarding, and end-to-end observability. v0.0.1 is the stability line: predictable routing, auditable execution, and resilient operations across channels.

## Immediate (next 2 weeks)
1. **Gateway readiness sweep**: tighten WebSocket frame validation, event ordering, and gap recovery.
2. **Routing hardening**: ensure deterministic reply-back with full session scoping and group isolation.
3. **Security gate polish**: upgrade approval prompts, audit entries, and allowlist enforcement.
4. **Docs integrity**: publish a Gateway quick-start with validated examples and a troubleshooting section.

## Short-term (1-3 months)
1. **Channel onboarding**: complete pairing flows for Slack and Telegram with re-pair recovery.
2. **Observability backbone**: add structured traces for routing, approvals, and tool execution.
3. **Operational durability**: health checks, self-healing hooks, and drift detection for Gateway runtime.
4. **Protocol compliance suite**: formalize gateway compliance tests and publish the conformance matrix.

## Medium-term (3-6 months)
1. **Cross-channel translation**: message and media translation layer with loss reporting.
2. **Context management**: production rollout of Distill for long sessions with agent-aware summarization.
3. **Governance model**: explicit capability boundaries with policy versioning.
4. **UI surface**: Control UI live traces, sessions, and tool approvals.

## Long-term (6-12 months)
1. **Multi-region control plane**: active-active Gateway instances with session affinity.
2. **Enterprise compliance**: SOC2-ready audit exports, retention policies, and approvals ledger.
3. **Channel expansion**: Discord, Signal, Matrix, and Teams adapters.
4. **Automation platform**: scheduled tasks, webhooks, and event-driven workflows at scale.

## Dependencies
- Gateway JSON schema coverage must be complete for all WS methods and events.
- Session storage must support deterministic reply-back and multi-session isolation.
- Approval history storage must be append-only and tamper-evident.

## Success Criteria
- 99.9% Gateway uptime over 30 days
- Median routing decision latency under 50ms
- Zero unaudited tool executions
- 100% of channel messages routed with deterministic reply-back
- End-to-end trace available for every interaction
