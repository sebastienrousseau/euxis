# Security Essentials

Security in Euxis is enforced by explicit policy defaults and audited approvals.

## Gateway Policy Defaults

Gateway security policy defaults live in:

`~/.euxis/security/gateway.json`

Key controls include:
- **Execution policy** (`policy`, `ask`, `ask_fallback`)
- **Allowlists** for agents, senders, and voice commands
- **Approval gates** for elevated or risky actions

## Approval Flow

1. An agent requests a tool execution.
2. The Gateway evaluates the request against the policy.
3. If required, an approval prompt is issued and logged.
4. On approval, execution continues; otherwise it is blocked.

## Audit Trail

All approval events are logged by the Gateway for traceability.

## Next Steps

See [Gateway Config](../reference/gateway-config.md) for full policy keys and defaults.
