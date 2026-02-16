# Security

Security owns approval policies, allowlists, and audit configuration.

## Purpose
This module defines the policy surface that guards execution and audits. It provides the default Gateway security configuration and policy documentation. It does not execute approvals or logging logic (those live in `gateway/`).

## Structure
- `gateway.json` — Gateway approval policy defaults and allowlists

## Dependencies
- `config/` — shared schemas
- `core/` — validation helpers

## Usage
```bash
jq '.' security/gateway.json
```

## Development
Update policy defaults here, then restart the Gateway.

## Testing
Policy validation runs during `euxis-certify`.

## API / Exports
Security exports `security/gateway.json` for the Gateway runtime.
