# Playbook Contract: Red Alert
## Metadata
- ID: red-alert
- Goal: Emergency response and recovery.
- Safety Tier: P0 (Critical)

## Crisis Management
- Kill-Switch: ENABLED
- Rollback Threshold: 5000ms
- NHI Scope: emergency_responder

## Execution DAG
### Phase 1: Triage
- **Agent:** Auditor
- **Contract:** Assess the incident impact and identify the root cause.
- **Success Criteria:** Root cause identified and severity assessed.
- **On Failure:** ABORT and trigger Kill-Switch.

### Phase 2: Repair
- **Agent:** Repairer
- **Contract:** Apply emergency fixes and verify via regression suite.
- **Success Criteria:** Fix deployed and verified.
- **On Failure:** TRIGGER ROLLBACK.

### Phase 3: Post-Mortem
- **Agent:** Strategist
- **Contract:** Document prevention strategy and update procedural memory.
