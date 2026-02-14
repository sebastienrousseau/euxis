# Playbooks

## What They Are

Playbooks are multi-phase workflows that orchestrate squads sequentially to accomplish complex, end-to-end goals. Each phase deploys a specialized squad, validates checkpoint criteria, and either proceeds or aborts based on success.

## When to Use

- **Complex workflows**: Multi-stage processes requiring different expertise at each phase
- **Quality gates**: Validated checkpoints between phases ensure quality progression
- **Repeatable processes**: Standardize proven workflows for common business goals
- **Risk management**: Abort-on-failure prevents cascading issues from early phase problems

## Quick Example

```bash
# Execute full product launch playbook
euxis-playbook run zero-to-one "Build SaaS dashboard for project management"

# Preview phases without execution
euxis-playbook run legacy-overhaul "Modernize authentication system" --dry-run

# Check playbook progress
euxis-playbook status 20260204-143022

# List available playbooks
euxis-playbook list
```

## Available Playbooks

### Zero to One
**Phases**: Vision → Build → Quality → Growth
**Goal**: Full product launch from concept to market

1. **Vision Squad**: Research and architect initial blueprint
2. **Build Squad**: Develop MVP based on vision
3. **Quality Squad**: Review, audit, and certify
4. **Growth Squad**: Document, market, and launch

**Use for**: New feature development, product launches, greenfield projects

### Legacy Overhaul
**Phases**: Assessment → Planning → Migration → Validation
**Goal**: Modernize existing systems without breaking production

**Use for**: Tech debt reduction, system modernization, architecture updates

### Red Alert
**Phases**: Incident Response → Root Cause → Fix → Prevention
**Goal**: Handle production incidents with full lifecycle management

**Use for**: Production outages, security incidents, critical system failures

### Verify Everything
**Phases**: Static Analysis → Dynamic Testing → Security Audit → Compliance
**Goal**: Comprehensive validation before major releases

**Use for**: Pre-release validation, compliance audits, security reviews

### Crypto Audit
**Phases**: Implementation Review → Threat Modeling → Penetration Testing → Compliance
**Goal**: Cryptographic security validation

**Use for**: Cryptographic systems, security-critical components, compliance requirements

### Audio Pipeline
**Phases**: Requirements → Implementation → Testing → Integration
**Goal**: Build and validate real-time audio processing systems

**Use for**: Audio processing features, real-time streaming, codec implementations

### Payments Integration
**Phases**: Security Review → Implementation → Compliance → Go-Live
**Goal**: Secure payment system integration with compliance validation

**Use for**: Payment gateway integrations, financial transactions, PCI compliance

### Rust Release
**Phases**: Code Review → Testing → Documentation → Publishing
**Goal**: Validate and publish Rust crate releases

**Use for**: Rust library releases, crate publishing, API documentation

## Playbook Structure

### Phase Definition
Each phase contains:
- **Squad**: Which team executes this phase
- **Mode**: Execution mode (hierarchical/mesh/federated)
- **Task Template**: Template with variable substitution
- **Checkpoint**: Success criteria to proceed
- **Abort Policy**: Whether to halt on failure

### Example: Zero to One
```json
{
  "phase": 1,
  "name": "Vision",
  "squad": "vision",
  "mode": "hierarchical",
  "task_template": "Research and architect the initial blueprint for: ${goal}",
  "checkpoint": "Architecture and requirements approved",
  "abort_on_failure": true
}
```

## Create Your Own

### Custom Playbook

Create `/config/playbooks/my-workflow.json`:

```json
{
  "schema_version": "1.0",
  "id": "api-development",
  "name": "API Development",
  "description": "End-to-end API development with documentation",
  "phases": [
    {
      "phase": 1,
      "name": "Design",
      "squad": "vision",
      "mode": "hierarchical",
      "task_template": "Design API specification and architecture for: ${goal}",
      "checkpoint": "OpenAPI spec approved and documented",
      "abort_on_failure": true
    },
    {
      "phase": 2,
      "name": "Implementation",
      "squad": "build",
      "mode": "hierarchical",
      "task_template": "Implement API endpoints based on specification: ${goal}",
      "checkpoint": "All endpoints implemented with unit tests",
      "abort_on_failure": true
    },
    {
      "phase": 3,
      "name": "Testing",
      "squad": "quality",
      "mode": "mesh",
      "task_template": "Comprehensive API testing and security review: ${goal}",
      "checkpoint": "All tests passing, security approved",
      "abort_on_failure": true
    },
    {
      "phase": 4,
      "name": "Documentation",
      "squad": "growth",
      "mode": "hierarchical",
      "task_template": "Create API documentation and developer guides: ${goal}",
      "checkpoint": "Documentation published and reviewed",
      "abort_on_failure": false
    }
  ]
}
```

### Execute Custom Playbook

```bash
# Run your custom playbook
euxis-playbook run api-development "User authentication API with OAuth2"

# Monitor progress
euxis-playbook status

# View playbook details
euxis-playbook info api-development
```

## Phase Execution

### Sequential Processing
- Phases execute one at a time in order
- Next phase only starts if previous phase succeeds
- Checkpoint validation occurs between phases

### Variable Substitution
- `${goal}` is replaced with the user-provided goal
- Custom variables can be added to task templates
- Variables are consistent across all phases

### Error Handling
- **abort_on_failure: true**: Stops playbook execution on phase failure
- **abort_on_failure: false**: Logs warning but continues to next phase
- Failed phases are logged in session status

## Playbook Management

```bash
# List all playbooks
euxis-playbook list

# Get playbook details and phases
euxis-playbook info <playbook-id>

# Execute with dry run preview
euxis-playbook run <playbook-id> "<goal>" --dry-run

# Check execution status
euxis-playbook status [session-id]

# View session logs
euxis-playbook status 20260204-143022
```

## Quality Gates

### Checkpoint Validation
Each phase defines success criteria that must be met:
- Architecture approved
- All tests passing
- Security review completed
- Documentation published

### Abort Policies
Critical phases (design, implementation) typically abort on failure.
Non-critical phases (documentation, marketing) may continue with warnings.

## Available Options

```bash
# Execute specific playbook
euxis-playbook run <playbook-id> "<goal>"

# Preview without execution
euxis-playbook run <playbook-id> "<goal>" --dry-run

# Override squad mode for all phases
euxis-playbook run <playbook-id> "<goal>" --mode mesh

# Status tracking
euxis-playbook status [session-id]
```

**See Also**: [Squads](squads.md) for phase execution teams, [Combos](combos.md) for sequential agent chains.