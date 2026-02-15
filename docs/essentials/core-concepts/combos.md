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

### Steve Jobs
**Chain**: `product-manager` → `architect` → `brand-evangelist` → `reviewer`
**Outcome**: Minimalist design with polished review
**Use for**: Product design, user experience, minimalist interfaces

Process flow:
1. **Product Manager**: Define clear user intent and requirements
2. **Architect**: Structure elegant technical solution
3. **Brand Evangelist**: Apply consistent brand voice and aesthetics
4. **Reviewer**: Quality gate and final polish

### Fort Knox
**Chain**: `edge-hunter` → `compliance-officer` → `qa-coordinator` → `reviewer`
**Outcome**: Maximum security assurance
**Use for**: Security-critical systems, compliance validation, audit preparation

Process flow:
1. **Edge Hunter**: Identify vulnerabilities and attack vectors
2. **Compliance Officer**: Validate regulatory requirements
3. **QA Coordinator**: Comprehensive testing strategy
4. **Reviewer**: Security and compliance validation

### Content Factory
**Chain**: `tech-writer` → `brand-evangelist` → `social-manager` → `reviewer`
**Outcome**: End-to-end content production
**Use for**: Documentation, tutorials, marketing content, developer resources

Process flow:
1. **Tech Writer**: Create accurate technical content
2. **Brand Evangelist**: Apply brand voice and messaging
3. **Social Manager**: Optimize for audience engagement
4. **Reviewer**: Quality and accuracy validation

### Jony Ive
**Chain**: `web-ui-architect` → `theming-and-motion-engineer` → `interaction-and-input-specialist` → `ux-sentinel` → `reviewer`
**Outcome**: Apple-level UI with polished interaction design
**Use for**: User interfaces, design systems, interaction patterns

Process flow:
1. **Web UI Architect**: Design component architecture
2. **Theming Engineer**: Apply visual design and motion
3. **Interaction Specialist**: Optimize input and navigation
4. **UX Sentinel**: User experience validation
5. **Reviewer**: Design and usability review

### Crypto Fortress
**Chain**: `security-lead` → `crypto-cryptography-auditor` → `edge-hunter` → `reviewer`
**Outcome**: Deep cryptographic audit with security policy
**Use for**: Cryptographic systems, security protocols, key management

### SWIFT Payment
**Chain**: `payments-domain-steward` → `compliance-officer` → `unit-tester` → `reviewer`
**Outcome**: Payments integration with compliance validation
**Use for**: Payment processing, financial systems, regulatory compliance

### Deep Review
**Chain**: `language-specialist` → `regression-analyst` → `edge-hunter` → `reviewer`
**Outcome**: Exhaustive pre-merge analysis
**Use for**: Code review, merge requests, quality assurance

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

Add to `squads.json` combos section:

```json
{
  "id": "performance-optimizer",
  "name": "Performance Optimizer",
  "description": "End-to-end performance optimization",
  "chain": [
    "perf-optimizer",
    "data-steward",
    "automation-engineer",
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
      "compliance-officer",
      "edge-hunter",
      "system-critic",
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
`specialist` → `reviewer` → `system-critic` → `final-reviewer`

### Domain Deep-Dive
Domain expert followed by complementary specialists:
`domain-expert` → `security-specialist` → `performance-expert` → `reviewer`

### Creative to Technical
Innovation to implementation flow:
`product-manager` → `architect` → `bug-fixer` → `reviewer`

### Risk Management
Multiple risk perspectives before approval:
`edge-hunter` → `system-critic` → `compliance-officer` → `reviewer`

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