# Euxis OpenClaw Bridge

This document defines the implementation plan and initial artifacts to make Euxis interoperable with OpenClaw-style skills while preserving Euxis security defaults.

## Scope

- OpenClaw compatibility for skill discovery and invocation patterns.
- Migration path from `~/.openclaw` to `~/.euxis` runtime data.
- Messaging bridge daemon to emulate always-on remote control channels.
- Mandatory security controls for signed, policy-gated execution.

## 5-Hour Sprint Outputs

1. Structural audit + top-20 skill mapping: [euxis-openclaw-bridge-sprint.md](euxis-docs/docs/reports/euxis-openclaw-bridge-sprint.md)
2. Migration-tool specification (OpenClaw -> Euxis identity/memory import): [euxis-openclaw-bridge-sprint.md](euxis-docs/docs/reports/euxis-openclaw-bridge-sprint.md)
3. Bridge daemon design (Telegram/Signal -> Euxis gateway/TUI): [euxis-openclaw-bridge-sprint.md](euxis-docs/docs/reports/euxis-openclaw-bridge-sprint.md)
4. Script-signing policy layer using Euxis crypto primitives: [euxis-openclaw-bridge-sprint.md](euxis-docs/docs/reports/euxis-openclaw-bridge-sprint.md)
5. Runtime instruction set + configuration profile:
   - Prompt: `euxis-ops/bridge/system_prompt_openclaw_bridge.txt`
   - Config: `bridge_config.yaml`

## Quick Start

1. Review config:
   - `bridge_config.yaml`
2. Review policy and mappings:
   - `euxis-docs/docs/reports/euxis-openclaw-bridge-sprint.md`
3. Adopt the runtime instruction set:
   - `euxis-ops/bridge/system_prompt_openclaw_bridge.txt`
4. Deploy daemon with platform runbook:
   - `euxis-docs/docs/guides/bridge-daemon.md`

## CLI Entry Points

- `euxis bridge import-openclaw --source ~/.openclaw --dry-run`
- `euxis bridge import-clawhub --source ~/.openclaw/skills --output ~/.euxis/euxis-data/bridge/clawhub-registry.json`
- `euxis bridge daemon --config bridge_config.yaml`
- `euxis bridge keygen --private-key ~/.euxis/euxis-security/keys/bridge-private.pem --public-key ~/.euxis/euxis-security/keys/bridge-public.pem`
- `euxis bridge sign-script --script ./script.sh --private-key ~/.euxis/euxis-security/keys/bridge-private.pem --sig ./script.sh.sig`
- `euxis bridge verify-script --script ./script.sh --public-key ~/.euxis/euxis-security/keys/bridge-public.pem --sig ./script.sh.sig`
- `euxis bridge signed-exec --script ./script.sh --sig ./script.sh.sig --public-key ~/.euxis/euxis-security/keys/bridge-public.pem -- ./script.sh`

## Compatibility Intent

The bridge targets operational parity for:

- Skill registry semantics (`SKILL.md` frontmatter + folder bundles)
- Workspace skill precedence and override behavior
- Persistent session/memory import and replay into Euxis runtime stores
- 24/7 inbound channel control routed to Euxis gateway/TUI

## Security Position

Unlike permissive skill execution patterns, Euxis bridge mode enforces:

- Signed script verification before tool execution
- Policy gates on network, filesystem, and subprocess scopes
- Explicit allowlists for remote channels and origin identities
- Replay-safe migration with checksums and provenance logs
