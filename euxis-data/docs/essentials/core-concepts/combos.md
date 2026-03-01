# Combos

## What They Are

Combos are predefined sequential chains of agents optimized for specific patterns and outcomes. Each combo represents a proven workflow where the output of one agent becomes input for the next, culminating in a polished final result.

## When to Use

- **Design patterns**: Apply proven agent sequences for common scenarios
- **Quality amplification**: Chain specialists to progressively refine output
- **Specific outcomes**: Target precise end goals like "Apple-level design" or "maximum security"
- **Workflow optimization**: Use battle-tested sequences instead of ad-hoc coordination

## Quick Example

```bash
# Apple-style design workflow
euxis-combo run envision "Design minimalist login interface"

# Maximum security assurance
euxis-combo run protect "Audit OAuth2 implementation"

# End-to-end content production
euxis-combo run craft "Create API tutorial with examples"

# View combo details
euxis-combo info refine
```

## Available Combos

### Envision
**Chain**: `deep-researcher` → `planner` → `architect` → `evangelist` → `reviewer`
**Outcome**: Product narrative aligned with architecture and ready for launch review
**Use for**: Product strategy, roadmap briefs, launch storytelling

Process flow:
1. **Deep Researcher**: Establish evidence-backed market and technical context
2. **Planner**: Define clear intent and acceptance criteria
3. **Architect**: Shape system design and boundary decisions
4. **Evangelist**: Refine narrative and launch messaging
5. **Reviewer**: Final quality gate and polish

### Protect
**Chain**: `sentinel` → `pentester` → `auditor` → `inspector` → `reviewer`
**Outcome**: Security posture hardened with evidence-backed assurance
**Use for**: Security-critical systems, compliance validation, audit preparation

Process flow:
1. **Sentinel**: Define policy boundaries and threat posture
2. **Pentester**: Identify vulnerabilities and attack vectors
3. **Auditor**: Validate regulatory and compliance requirements
4. **Inspector**: Verify E2E flows and evidence quality
5. **Reviewer**: Security and compliance validation

### Craft
**Chain**: `writer` → `strategist` → `evangelist` → `reviewer`
**Outcome**: End-to-end content production with consistent voice
**Use for**: Documentation, tutorials, marketing content, developer resources

Process flow:
1. **Writer**: Create accurate technical content
2. **Strategist**: Align message, audience, and distribution
3. **Evangelist**: Apply brand voice and narrative consistency
4. **Reviewer**: Quality and accuracy validation

### Refine
**Chain**: `designer` → `animator` → `interactor` → `reviewer`
**Outcome**: Interface refined from system to motion and interaction
**Use for**: User interfaces, design systems, interaction patterns

Process flow:
1. **Designer**: Shape component architecture and layout
2. **Animator**: Apply motion and visual rhythm
3. **Interactor**: Optimize input and navigation
4. **Reviewer**: Design and usability review

### Seal
**Chain**: `sentinel` → `cryptographer` → `pentester` → `reviewer`
**Outcome**: Deep cryptographic audit with security policy
**Use for**: Cryptographic systems, security protocols, key management

### Settle
**Chain**: `ledger` → `auditor` → `tester` → `reviewer`
**Outcome**: Payments integration with compliance validation
**Use for**: Payment processing, financial systems, regulatory compliance

### Inspect
**Chain**: `researcher` → `polyglot` → `watchdog` → `pentester` → `reviewer`
**Outcome**: Exhaustive pre-merge analysis
**Use for**: Code review, merge requests, quality assurance

### Discover
**Chain**: `deep-researcher` → `architect` → `reviewer`
**Outcome**: Evidence-backed architecture roadmap
**Use for**: Platform decisions, modernization plans, research synthesis

## Sequential Execution

### Chain Processing
- Each agent in chain receives task and previous agent output
- Output flows sequentially: Agent 1 → Agent 2 → Agent 3 → Final
- Final `reviewer` agent validates complete chain output

### Context Preservation
- Task context preserved throughout chain
- Previous outputs inform subsequent agents
- Final output synthesizes all chain contributions

### Error Handling
- Chain stops on agent failure
- Failed step logged with specific agent and error
- Partial output available for debugging

## Create Your Own

### Define Custom Combo

Add to `agents/squads.json` combos section:

```json
{
  "id": "performance-optimizer",
  "name": "Performance Optimizer",
  "description": "End-to-end performance optimization",
  "chain": [
    "optimizer",
    "telemetrist",
    "automaton",
    "reviewer"
  ]
}
```

### Alternative: Manifest Approach

Create standalone combo file:

```json
{
  "schema_version": "1.0",
  "combo": {
    "id": "ai-safety",
    "name": "AI Safety Validator",
    "description": "Comprehensive AI safety and ethics review",
    "chain": [
      "auditor",
      "pentester",
      "critic",
      "reviewer"
    ]
  }
}
```

### Execute Custom Combo

```bash
# Register and run custom combo
euxis-combo run performance-optimizer "Optimize database queries for user dashboard"

# Test with different providers
euxis-combo run ai-safety "Review ML model training pipeline" --provider gemini
```

## Combo Patterns

### Quality Amplification
Progressive refinement through specialist chain:
`polyglot` → `watchdog` → `reviewer` → `arbiter`

### Domain Deep-Dive
Domain expert followed by complementary specialists:
`cryptographer` → `sentinel` → `pentester` → `reviewer`

### Creative to Technical
Innovation to implementation flow:
`planner` → `architect` → `debugger` → `reviewer`

### Risk Management
Multiple risk perspectives before approval:
`sentinel` → `pentester` → `auditor` → `reviewer`

## Combo Management

```bash
# List all available combos
euxis-combo list

# View combo chain details
euxis-combo info <combo-id>

# Execute combo with task
euxis-combo run <combo-id> "<task>" [--provider P]

# Validate combo agents exist
euxis-combo validate
```

## Agent Flow Optimization

### Chain Length
- **3-4 agents**: Optimal for most workflows
- **5+ agents**: Use for complex multi-domain tasks
- **2 agents**: Minimal for simple enhancement patterns

### Agent Selection
- **Lead with domain expert**: Start with specialist most relevant to task
- **End with reviewer**: Always finish with quality validation
- **Balance perspectives**: Mix different agent tiers and specialties

### Chain Validation
- All agents in chain must exist in registry
- Chain order matters for context flow
- Reviewer agent should always be final for quality gate

## Available Options

```bash
# Execute combo
euxis-combo run <combo-id> "<task>"

# Override model provider for entire chain
euxis-combo run <combo-id> "<task>" --provider gemini

# List combos with chain details
euxis-combo list --verbose

# Validate combo definitions
euxis-combo validate
```

**See Also**: [Agents](agents.md) for individual specialists, [Squads](squads.md) for parallel team coordination, [Playbooks](playbooks.md) for multi-phase workflows.
