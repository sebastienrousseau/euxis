# Templates

## What They Are

Templates are standardized file formats for creating new Euxis components. They ensure consistency across agent prompts, dispatch manifests, playbooks, patterns, and documentation.

## When to Use

- **New agents**: Use agent-prompt.txt as a starting point
- **Dispatch manifests**: Follow dispatch-manifest.json schema
- **Playbooks**: Use playbook.json for multi-phase workflows
- **Patterns**: Create validation patterns with pattern.md
- **ADRs**: Document decisions with adr.md

## Quick Example

```bash
# List available templates
ls ~/.euxis/config/templates/

# Create a new agent from template
cp ~/.euxis/config/templates/agent-prompt.txt \
   ~/.euxis/agents/prompts/fleet/my-new-agent.txt
```

## Available Templates

### agent-prompt.txt
**Purpose:** Create new agent prompts with proper YAML frontmatter and structure.

```yaml
---
agent_id: "agent-name"
role: "Agent role description"
version: "0.0.2"
tags: ["category1", "category2"]
last_updated: "current-02-04"
---

# Agent Name

## Mandate
[Core purpose and responsibility]

## Primary Directives
[Key behaviors and priorities]

## Scope Boundaries
[What the agent does and doesn't do]

## Output Format
[Expected deliverable structure]
```

### dispatch-manifest.json
**Purpose:** Define multi-agent dispatch configurations.

```json
{
  "project": "project-name",
  "description": "What this dispatch accomplishes",
  "dispatches": [
    {
      "agent": "agent-id",
      "priority": "P0",
      "task": "Task description",
      "verify_cmd": "command to verify completion"
    }
  ]
}
```

### playbook.json
**Purpose:** Define multi-phase squad workflows.

```json
{
  "schema_version": "1.0",
  "id": "playbook-id",
  "name": "Playbook Name",
  "description": "What this playbook accomplishes",
  "phases": [
    {
      "phase": 1,
      "name": "Phase Name",
      "squad": "squad-id",
      "mode": "hierarchical",
      "task_template": "Task with ${goal} variable",
      "checkpoint": "Success criteria",
      "abort_on_failure": true
    }
  ]
}
```

### pattern.md
**Purpose:** Define validation patterns for quality checks.

```markdown
# CATEGORY-NNN

## Pattern
[Pattern name and description]

## Severity
CRITICAL | HIGH | MEDIUM | LOW

## Detection Rules
1. First detection rule
2. Second detection rule

## Validation
- [ ] Checklist item 1
- [ ] Checklist item 2

## Remediation
[How to fix violations]
```

### adr.md
**Purpose:** Document architectural decisions.

```markdown
# ADR-NNN: Decision Title

## Status
PROPOSED | ACCEPTED | DEPRECATED | SUPERSEDED

## Context
[Why this decision is needed]

## Decision
[What was decided]

## Consequences
[Implications of the decision]
```

### domain-review-protocol.md
**Purpose:** Structured domain review checklist.

## Template Locations

| Template | Location | Used For |
|----------|----------|----------|
| agent-prompt.txt | config/templates/ | New agents |
| dispatch-manifest.json | config/templates/ | Dispatch configs |
| playbook.json | config/templates/ | Playbook definitions |
| pattern.md | config/templates/ | Validation patterns |
| adr.md | config/templates/ | Architecture decisions |
| domain-review-protocol.md | config/templates/ | Domain reviews |

## Using Templates

### Create New Agent

```bash
# Copy template
cp ~/.euxis/config/templates/agent-prompt.txt \
   ~/.euxis/agents/prompts/fleet/my-agent.txt

# Edit with your agent details
$EDITOR ~/.euxis/agents/prompts/fleet/my-agent.txt

# Register in agents/registry.json
# (add entry with agent ID matching filename)
```

### Create New Playbook

```bash
# Copy template
cp ~/.euxis/config/templates/playbook.json \
   ~/.euxis/config/playbooks/my-workflow.json

# Customize phases and goals
$EDITOR ~/.euxis/config/playbooks/my-workflow.json

# Test with dry run
euxis-playbook run my-workflow "Test goal" --dry-run
```

### Create New Pattern

```bash
# Copy template
cp ~/.euxis/config/templates/pattern.md \
   ~/.euxis/config/patterns/MYCAT-001.md

# Define detection rules
$EDITOR ~/.euxis/config/patterns/MYCAT-001.md

# Validate pattern format
euxis-lint
```

## Best Practices

1. **Always start from templates**: Ensures proper structure
2. **Keep templates updated**: Reflect current schema versions
3. **Validate after creation**: Run lint/certify checks
4. **Document customizations**: Note deviations from template

**See Also**: [Agents](agents.md), [Playbooks](playbooks.md), [Patterns](../guides/user-guide.md#patterns)
