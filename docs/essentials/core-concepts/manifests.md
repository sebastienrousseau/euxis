# Dispatch Manifests

## What They Are

Dispatch manifests are JSON configuration files that define multi-agent task deployments. They specify which agents to deploy, their priorities, tasks, and verification commands.

## When to Use

- **Parallel deployments**: Deploy multiple agents simultaneously
- **Complex workflows**: Coordinate agents with dependencies
- **Repeatable processes**: Standardize multi-agent operations
- **Verification requirements**: Ensure tasks complete successfully

## Quick Example

```bash
# Deploy from manifest
euxis-dispatch config/manifests/doc-overhaul.json

# Create a simple manifest
cat > /tmp/review-manifest.json << 'EOF'
{
  "project": "code-review",
  "dispatches": [
    {"agent": "reviewer", "priority": "P0", "task": "Review authentication module"},
    {"agent": "security-lead", "priority": "P1", "task": "Security audit"}
  ]
}
EOF

euxis-dispatch /tmp/review-manifest.json
```

## Manifest Schema

```json
{
  "project": "project-name",
  "description": "Optional description of what this manifest accomplishes",
  "dispatches": [
    {
      "agent": "agent-id",
      "priority": "P0|P1|P2|P3",
      "task": "Task description for the agent",
      "verify_cmd": "shell command to verify completion"
    }
  ]
}
```

### Fields

| Field | Required | Description |
|-------|----------|-------------|
| `project` | Yes | Project name for output organization |
| `description` | No | Human-readable manifest purpose |
| `dispatches` | Yes | Array of agent dispatch configurations |
| `dispatches[].agent` | Yes | Agent ID from registry |
| `dispatches[].priority` | Yes | P0 (highest) to P3 (lowest) |
| `dispatches[].task` | Yes | Task description sent to agent |
| `dispatches[].verify_cmd` | No | Shell command to verify completion |

### Priority Levels

| Priority | Meaning | Execution |
|----------|---------|-----------|
| P0 | Critical | Executes first, blocks on failure |
| P1 | High | Executes after P0 completes |
| P2 | Medium | Executes after P1 completes |
| P3 | Low | Executes last |

## Execution Modes

### Hierarchical (Default)
Agents execute in priority order. P0 completes before P1 starts.

```bash
euxis-dispatch manifest.json --mode hierarchical
```

### Mesh
All agents execute in parallel regardless of priority.

```bash
euxis-dispatch manifest.json --mode mesh
```

### Federated
Agents vote on decisions, useful for consensus-based tasks.

```bash
euxis-dispatch manifest.json --mode federated
```

## Verification Commands

The `verify_cmd` field specifies how to verify task completion:

```json
{
  "agent": "tech-writer",
  "task": "Create quick-start guide",
  "verify_cmd": "test -f docs/quick-start.md && wc -l docs/quick-start.md | awk '$1 > 50'"
}
```

**Common patterns:**
- File existence: `test -f path/to/file`
- Content check: `grep -q 'pattern' file`
- Line count: `wc -l file | awk '$1 > N'`
- Command success: `command && echo pass`

## Examples

### Documentation Audit

```json
{
  "project": "doc-audit",
  "description": "Comprehensive documentation review",
  "dispatches": [
    {
      "agent": "librarian",
      "priority": "P0",
      "task": "Audit all documentation for gaps and inconsistencies",
      "verify_cmd": "test -f /tmp/doc-audit-report.md"
    },
    {
      "agent": "tech-writer",
      "priority": "P1",
      "task": "Create missing quick-start guide",
      "verify_cmd": "test -f docs/quick-start.md"
    },
    {
      "agent": "reviewer",
      "priority": "P2",
      "task": "Review all documentation changes",
      "verify_cmd": "euxis-certify 2>&1 | grep -q 'CERTIFIED'"
    }
  ]
}
```

### Security Review

```json
{
  "project": "security-review",
  "description": "Comprehensive security audit",
  "dispatches": [
    {
      "agent": "security-lead",
      "priority": "P0",
      "task": "Identify potential vulnerabilities in authentication"
    },
    {
      "agent": "edge-hunter",
      "priority": "P1",
      "task": "Test edge cases and attack vectors"
    },
    {
      "agent": "compliance-officer",
      "priority": "P2",
      "task": "Verify compliance with security standards"
    }
  ]
}
```

## Stored Manifests

Pre-built manifests are stored in `config/manifests/`:

```bash
ls ~/.euxis/config/manifests/
# doc-overhaul.json
```

Create your own:

```bash
# Create manifest
cat > ~/.euxis/config/manifests/my-workflow.json << 'EOF'
{
  "project": "my-project",
  "dispatches": [...]
}
EOF

# Execute
euxis-dispatch config/manifests/my-workflow.json
```

## Monitoring Dispatch

```bash
# Watch dispatch progress
euxis-dispatch manifest.json

# Check logs
ls /tmp/euxis_dispatch_*/

# View specific agent log
cat /tmp/euxis_dispatch_*/agent-name_*.log
```

## Best Practices

1. **Start with P0 blockers**: Critical setup tasks should block
2. **Use verification**: Always add verify_cmd for important tasks
3. **Keep tasks focused**: One clear objective per dispatch
4. **Test with dry-run**: Preview before executing

**See Also**: [Squads](squads.md), [Playbooks](playbooks.md), [CLI Reference](../reference/cli-reference.md#euxis-dispatch)
