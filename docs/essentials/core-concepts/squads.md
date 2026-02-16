# Squads

## What They Are

Squads are pre-configured teams of agents with complementary skills, a designated lead, and a shared purpose. They provide ready-made coordination for common workflows without manual agent selection.

## When to Use

- **Domain expertise cluster**: Deploy multiple specialists for comprehensive coverage
- **Balanced perspective**: Get input from different agent tiers and specialties
- **Proven combinations**: Use battle-tested agent groupings for standard workflows
- **Parallel execution**: Execute multiple agents simultaneously via dispatch manifest

## Quick Example

```bash
# Deploy entire build squad for engineering tasks
euxis-squad deploy build "Implement OAuth2 authentication with tests"

# Get squad details and member list
euxis-squad info quality
euxis-squad members vision

# List all available squads
euxis-squad list
```

## Available Squads

### Vision Squad
**Purpose**: Strategy, Research, and Architecture
**Lead**: `orchestrator`
**Members**: `orchestrator`, `architect`, `planner`, `deep-researcher`, `researcher`, `historian`, `accountant`

Use for: Requirements gathering, system design, strategic planning, research

### Build Squad
**Purpose**: Engineering & Execution
**Lead**: `debugger`
**Members**: `debugger`, `maintainer`, `automaton`, `tester`, `investigator`, `repairer`

Use for: Feature development, bug fixes, infrastructure, testing, maintenance

### Quality Squad
**Purpose**: Assurance & Security
**Lead**: `reviewer`
**Members**: `reviewer`, `inspector`, `sentinel`, `pentester`, `auditor`, `optimizer`, `watchdog`, `polyglot`, `arbiter`

Use for: Code review, security audits, performance optimization, compliance validation

### Growth Squad
**Purpose**: Branding & Documentation
**Lead**: `writer`
**Members**: `writer`, `evangelist`, `strategist`, `ambassador`, `marketer`, `localizer`

Use for: Documentation, marketing materials, community outreach, content creation

### Experience Squad
**Purpose**: UI Excellence & Interaction Design
**Lead**: `designer`
**Members**: `designer`, `tactician`, `animator`, `interactor`

Use for: UI/UX design, interaction patterns, theming systems, user experience optimization

### Specialist Squad
**Purpose**: Domain-Specific Expertise
**Lead**: `cryptographer`
**Members**: `cryptographer`, `ledger`, `conduit`, `custodian`

Use for: Cryptography, payments, audio processing, Rust ecosystem tasks

## Squad Execution Modes

Squads execute via dispatch manifests in three modes:

### Hierarchical (Default)
- All agents report to squad lead
- Central coordination through lead agent
- Best for: Complex tasks requiring tight synchronization

### Mesh
- Agents coordinate directly with each other
- Specialists manage sub-workflows autonomously
- Best for: Focused sub-workflows with domain expertise

### Federated
- Agents operate independently across boundaries
- Minimal central coordination
- Best for: Cross-project or distributed tasks

## Create Your Own

### Custom Squad Definition

Add to `squads.json`:

```json
{
  "id": "security-fortress",
  "name": "Security Fortress Squad",
  "purpose": "Maximum Security Assurance",
  "lead": "sentinel",
  "members": [
    "sentinel",
    "pentester",
    "cryptographer",
    "auditor",
    "reviewer"
  ]
}
```

### Deploy Custom Squad

```bash
# Create dispatch manifest for your squad
cat > security-audit-manifest.json <<'EOF'
{
  "project": "security-audit",
  "mode": "hierarchical",
  "dispatches": [
    {
      "agent": "sentinel",
      "priority": "P0",
      "task": "Lead comprehensive security audit of authentication system",
      "verify_cmd": "test -f /tmp/security-report.md"
    },
    {
      "agent": "pentester",
      "priority": "P0",
      "task": "Perform boundary testing and vulnerability assessment",
      "verify_cmd": "grep -q 'SCAN COMPLETE' /tmp/pentester-log.txt"
    },
    {
      "agent": "cryptographer",
      "priority": "P1",
      "task": "Audit cryptographic implementation and key management",
      "verify_cmd": "test -f /tmp/crypto-audit.md"
    }
  ]
}
EOF

# Execute squad via dispatch
euxis-dispatch security-audit-manifest.json
```

## Squad Management

```bash
# List all squads with member counts
euxis-squad list

# Get detailed squad information
euxis-squad info <squad-id>

# View squad members with registry details
euxis-squad members <squad-id>

# Validate all squad members exist in registry
euxis-squad validate

# Deploy squad with custom task
euxis-squad deploy <squad-id> "<task description>"
```

## Authority & Coordination

- **Squad lead** coordinates execution and resolves conflicts
- **Core agents** in squad maintain blocking authority for quality
- **Mixed tiers** provide balanced perspective (authority + execution + capability)
- **Escalation** follows individual agent escalation protocols

## Available Options

```bash
# Deploy squad in specific mode
euxis-squad deploy vision "Research blockchain scalability" --mode mesh

# Deploy with provider override
euxis-squad deploy build "Optimize performance" --provider gemini

# Dry run to preview execution
euxis-squad deploy quality "Security review" --dry-run
```

**See Also**: [Playbooks](playbooks.md) for multi-phase workflows, [Agents](agents.md) for individual specialist reference.
