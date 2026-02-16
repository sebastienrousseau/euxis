# (c) 2026 Euxis Fleet. All rights reserved.
# Plugin Registration API

Complete guide for creating and registering custom agents in the Euxis system using the plugin API.

## Doc: Plugin Registration API Reference

### Metadata
- **Type:** Reference + How-To Guide
- **Audience:** Intermediate to Advanced
- **Prerequisites:**
  - Euxis system installed (`~/.euxis/` directory exists)
  - `jq` command-line JSON processor installed
  - Basic understanding of agent concepts

## Overview

The Euxis plugin system allows you to register third-party agents without modifying the core system. Plugins are registered via JSON manifests and integrate seamlessly with the existing agent fleet.

**Key Benefits:**
- Non-invasive: no core system modification required
- Automatic discovery: plugins appear in `euxis-health` and `list_agents` output
- Full feature support: plugins inherit all protocol features (ReAct, memory, audit trails)
- Easy management: register, unregister, and list plugins with simple commands

## Quick Start (5 Minutes)

### 1. Create Your Agent Prompt

First, create an agent prompt file following the standard format:

```bash
# Create a directory for your plugin
mkdir -p ~/my-plugins

# Create the agent prompt file
cat > ~/my-plugins/code-optimizer.txt <<'EOF'
---
agent_id: code-optimizer
role: "Performance optimization specialist for code review and refactoring"
version: "0.0.8"
tags: [performance, optimization, code-review]
last_updated: "2026-02-03"
---

# Euxis Agent: Code Optimizer

You are a **Code Optimization Specialist** within the Euxis system. You analyze code for performance bottlenecks and suggest improvements.

## Mandate

You MUST identify performance issues, memory inefficiencies, and algorithmic improvements in code. Focus on measurable optimizations with clear impact.

## Scope Boundaries

- **MUST NOT** make functional changes that alter business logic. Delegate to `debugger`.
- **MUST NOT** make architectural decisions. Delegate to `architect`.
- **MUST NOT** handle security vulnerabilities. Delegate to `pentester`.

## Primary Directives

1. **ANALYZE** code for O(n) complexity issues and suggest algorithmic improvements.
2. **IDENTIFY** memory leaks, excessive allocations, and inefficient data structures.
3. **RECOMMEND** specific optimizations with performance impact estimates.
4. **VALIDATE** that optimizations preserve existing functionality.

## Output Format

Every optimization analysis MUST use this structure:

```markdown
## Optimization Analysis

### Performance Issues Found
- **Issue:** [Description of the bottleneck]
- **Impact:** [Performance impact - High/Medium/Low]
- **Location:** [File:line reference]
- **Fix:** [Specific optimization recommendation]

### Suggested Changes
\`\`\`[language]
[Optimized code example]
\`\`\`

### Performance Gain
- **Before:** [Current performance characteristics]
- **After:** [Expected improvement with rationale]
```

## Escalation Protocol

- If code functionality is unclear, MUST request clarification from `architect`.
- If optimizations require breaking changes, MUST escalate to `planner`.
- If security implications exist, MUST flag to `pentester` before optimization.
EOF
```

### 2. Create the Plugin Manifest

```bash
cat > ~/my-plugins/code-optimizer-manifest.json <<'EOF'
{
  "agent_id": "code-optimizer",
  "role": "Performance optimization specialist for code review and refactoring",
  "prompt_file": "/home/seb/my-plugins/code-optimizer.txt",
  "tier": "standard",
  "tags": ["performance", "optimization", "code-review"]
}
EOF
```

### 3. Register the Plugin

```bash
# Source the agents library
source ~/.euxis/core/lib/agents.sh

# Register your plugin
register_agent_plugin ~/my-plugins/code-optimizer-manifest.json
```

### 4. Verify Registration

```bash
# Check if the agent is registered
euxis-health | grep code-optimizer

# List all plugins
source ~/.euxis/core/lib/agents.sh && list_plugins

# Test your agent
euxis code-optimizer "Analyze this function for performance issues: function slow_search(arr, target) { for(let i = 0; i < arr.length; i++) { if(arr[i] === target) return i; } return -1; }"
```

### 5. Unregister (If Needed)

```bash
source ~/.euxis/core/lib/agents.sh
unregister_agent_plugin "code-optimizer"
```

## Plugin Manifest Reference

### Required Fields

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `agent_id` | string | Unique identifier for your agent | `"code-optimizer"` |
| `role` | string | One-line description of agent purpose | `"Performance optimization specialist"` |
| `prompt_file` | string | Absolute path to agent prompt file | `"/home/user/my-agent.txt"` |

### Optional Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `tier` | string | `"standard"` | Intelligence tier: `"standard"`, `"strategic"`, `"research"`, `"coding"`, `"utility"` |
| `tags` | array | `[]` | Categorization tags for agent discovery |

### Complete Manifest Example

```json
{
  "agent_id": "api-documenter",
  "role": "Automated API documentation generator",
  "prompt_file": "/home/seb/my-plugins/api-documenter.txt",
  "tier": "utility",
  "tags": ["documentation", "api", "automation"]
}
```

## Agent Prompt Requirements

Your agent prompt file MUST include specific sections to work properly with Euxis:

### Required YAML Frontmatter

```yaml
---
agent_id: your-agent-name    # Must match manifest agent_id
role: "One-line description"
version: "0.0.8"            # Use current Euxis version
tags: [category1, category2]
last_updated: "YYYY-MM-DD"
---
```

### Required Sections

1. **Agent Title**: `# Euxis Agent: [Name]`
2. **Mandate**: What the agent does (one paragraph)
3. **Scope Boundaries**: What's in/out of scope with delegation targets
4. **Output Format**: Expected response structure
5. **Primary Directives** (recommended): Numbered list of key responsibilities

### Example Template

```markdown
---
agent_id: my-agent
role: "Brief description of what this agent does"
version: "0.0.8"
tags: [category]
last_updated: "2026-02-03"
---

# Euxis Agent: My Agent Name

You are a **[Role Title]** within the Euxis system. [One sentence about your purpose].

## Mandate

You MUST [primary responsibility]. [Additional context about the agent's role].

## Scope Boundaries

- **MUST NOT** [excluded responsibility]. Delegate to `[agent-name]`.
- **MUST NOT** [another exclusion]. Delegate to `[agent-name]`.

## Primary Directives

1. **[VERB]** [specific directive]
2. **[VERB]** [specific directive]

## Output Format

[Description of expected output structure, preferably with examples]

## Escalation Protocol

- If [condition], MUST [action] to `[agent-name]`.
```

## API Functions Reference

### register_agent_plugin

**Function:** `register_agent_plugin "manifest.json"`

**Description:** Registers a third-party agent from a JSON manifest file.

**Parameters:**
- `manifest` - Absolute path to JSON manifest file

**Behavior:**
1. Validates manifest has required fields (`agent_id`, `prompt_file`)
2. Checks that `prompt_file` exists and is readable
3. Creates symbolic link: `~/.euxis/prompts/fleet/${agent_id}.txt` → `prompt_file`
4. Copies manifest to `~/.euxis/config/plugins/${agent_id}.json`
5. Logs successful registration

**Dependencies:** Requires `jq` command

**Return Codes:**
- `0` - Success
- `1` - Failure (missing manifest, invalid JSON, missing prompt file, or missing jq)

**Example:**
```bash
source ~/.euxis/core/lib/agents.sh
register_agent_plugin "/path/to/my-plugin-manifest.json"
```

### unregister_agent_plugin

**Function:** `unregister_agent_plugin "agent_id"`

**Description:** Removes a registered plugin agent.

**Parameters:**
- `agent_id` - ID of the plugin to remove

**Behavior:**
1. Removes symlink from `~/.euxis/prompts/fleet/`
2. Removes manifest from `~/.euxis/config/plugins/`
3. Logs successful unregistration

**Example:**
```bash
source ~/.euxis/core/lib/agents.sh
unregister_agent_plugin "my-agent"
```

### list_plugins

**Function:** `list_plugins`

**Description:** Lists all registered plugin agents.

**Output:** Agent IDs with "(plugin)" suffix, one per line with indentation

**Example:**
```bash
source ~/.euxis/core/lib/agents.sh
list_plugins
# Output:
#     code-optimizer (plugin)
#     api-documenter (plugin)
```

## Advanced Examples

### Example 1: Database Migration Agent

This example shows a specialized agent for database migrations:

**Prompt File (`db-migrator.txt`):**
```markdown
---
agent_id: db-migrator
role: "Database migration specialist for schema changes and data transformations"
version: "0.0.8"
tags: [database, migration, schema]
last_updated: "2026-02-03"
---

# Euxis Agent: Database Migrator

You are a **Database Migration Specialist** within the Euxis system. You design, review, and validate database schema changes and data migrations.

## Mandate

You MUST ensure database migrations are safe, reversible, and maintain data integrity. Focus on zero-downtime deployments and rollback strategies.

## Scope Boundaries

- **MUST NOT** execute migrations directly. Delegate to `automaton`.
- **MUST NOT** make application code changes. Delegate to `debugger`.
- **MUST NOT** handle infrastructure scaling. Delegate to `optimizer`.

## Primary Directives

1. **DESIGN** migration scripts with proper up/down paths
2. **VALIDATE** data integrity before and after migrations
3. **ENSURE** migrations are idempotent and can be safely retried
4. **DOCUMENT** rollback procedures for every migration

## Output Format

```markdown
## Migration Plan: [Migration Name]

### Schema Changes
- [List of DDL changes]

### Data Transformations
- [List of data migration steps]

### Migration Script
\`\`\`sql
-- Up migration
[SQL statements]
\`\`\`

### Rollback Script
\`\`\`sql
-- Down migration
[Rollback SQL statements]
\`\`\`

### Validation Steps
- [ ] [Pre-migration validation]
- [ ] [Post-migration validation]
- [ ] [Rollback validation]

### Risk Assessment
- **Impact:** [High/Medium/Low]
- **Downtime:** [Estimated duration]
- **Rollback Time:** [Estimated duration]
```

## Escalation Protocol

- If migration affects application logic, MUST consult `architect`.
- If performance impact is unclear, MUST engage `optimizer`.
- If data security implications exist, MUST flag to `pentester`.
```

**Manifest File (`db-migrator-manifest.json`):**
```json
{
  "agent_id": "db-migrator",
  "role": "Database migration specialist for schema changes and data transformations",
  "prompt_file": "/home/seb/my-plugins/db-migrator.txt",
  "tier": "coding",
  "tags": ["database", "migration", "schema", "data-transformation"]
}
```

### Example 2: API Testing Agent

This example shows an agent specialized for API testing:

**Prompt File (`api-tester.txt`):**
```markdown
---
agent_id: api-tester
role: "API testing specialist for endpoint validation and contract testing"
version: "0.0.8"
tags: [testing, api, validation, contracts]
last_updated: "2026-02-03"
---

# Euxis Agent: API Tester

You are an **API Testing Specialist** within the Euxis system. You create comprehensive test suites for REST APIs, GraphQL endpoints, and service contracts.

## Mandate

You MUST ensure API endpoints are properly tested with comprehensive coverage including happy paths, edge cases, error conditions, and contract validation.

## Scope Boundaries

- **MUST NOT** write unit tests for business logic. Delegate to `tester`.
- **MUST NOT** handle performance testing. Delegate to `optimizer`.
- **MUST NOT** design API contracts. Delegate to `architect`.

## Primary Directives

1. **CREATE** comprehensive test suites for API endpoints
2. **VALIDATE** request/response schemas and data contracts
3. **TEST** error conditions and edge cases thoroughly
4. **ENSURE** API documentation matches actual behavior

## Output Format

```markdown
## API Test Suite: [Endpoint Name]

### Test Coverage Summary
- **Endpoints Covered:** [count]
- **Test Cases:** [count]
- **Edge Cases:** [count]
- **Error Scenarios:** [count]

### Test Cases

#### Happy Path Tests
\`\`\`javascript
describe('[Endpoint]', () => {
  it('should [behavior]', async () => {
    [test implementation]
  });
});
\`\`\`

#### Edge Case Tests
\`\`\`javascript
[Edge case test implementations]
\`\`\`

#### Error Condition Tests
\`\`\`javascript
[Error scenario test implementations]
\`\`\`

### Contract Validation
- [ ] Request schema validation
- [ ] Response schema validation
- [ ] Error response format validation
- [ ] Rate limiting behavior validation
```

## Escalation Protocol

- If API behavior contradicts documentation, MUST flag to `writer`.
- If security vulnerabilities are found, MUST escalate to `pentester`.
- If performance issues are discovered, MUST escalate to `optimizer`.
```

**Manifest File (`api-tester-manifest.json`):**
```json
{
  "agent_id": "api-tester",
  "role": "API testing specialist for endpoint validation and contract testing",
  "prompt_file": "/home/seb/my-plugins/api-tester.txt",
  "tier": "coding",
  "tags": ["testing", "api", "validation", "contracts", "automation"]
}
```

## Intelligence Tiers

Choose the appropriate tier for your agent based on its computational requirements:

| Tier | Provider | Use Case | Examples |
|------|----------|----------|----------|
| `strategic` | `claude` | Complex reasoning, architecture | System design agents |
| `research` | `gemini` | Deep analysis, massive context | Research agents |
| `coding` | `goose` | Agentic tool use, implementation | Code generation agents |
| `utility` | `ollama` | Simple tasks, formatting | Helper agents |
| `standard` | `claude` | Default tier | General purpose agents |

**Note:** The tier field in your manifest is informational. Actual provider assignment is controlled by the `resolve_tiered_provider()` function in `core/lib/providers.sh`.

## Best Practices

### 1. Naming Conventions
- Use kebab-case for agent IDs: `my-agent`, `code-optimizer`
- Keep IDs concise but descriptive
- Avoid conflicts with existing fleet agents

### 2. Scope Definition
- Clearly define what your agent MUST NOT do
- Provide specific delegation targets for out-of-scope work
- Use imperative language (MUST, MUST NOT, ALWAYS, NEVER)

### 3. Output Format
- Define structured output formats with examples
- Include validation checklists where appropriate
- Use markdown for consistent formatting

### 4. File Organization
- Keep plugin files organized in a dedicated directory
- Use absolute paths in manifests for reliability
- Include version numbers in prompt frontmatter

### 5. Testing
- Test registration and unregistration workflows
- Verify agent appears in health reports
- Test actual agent execution with sample tasks

## Troubleshooting

### Common Issues

**Plugin registration fails with "manifest not found"**
- Verify the manifest file path is absolute and accessible
- Check file permissions (must be readable)

**Plugin registration fails with "jq is required"**
- Install jq: `sudo apt install jq` (Ubuntu/Debian) or `brew install jq` (macOS)

**Agent appears as "dead" in health check**
- Verify prompt file path in manifest is correct and absolute
- Check prompt file permissions (must be readable)

**Agent appears as "not_ready" in health check**
- Ensure prompt has required frontmatter (starts with `---`)
- Verify required sections exist: "mandate" and "output format"
- Check that the assigned provider is installed

### Debugging Commands

```bash
# Check plugin registration status
source ~/.euxis/core/lib/agents.sh && list_plugins

# Full health report including plugins
euxis-health

# Check agent liveness specifically
source ~/.euxis/core/lib/agents.sh
agent_probe_liveness "your-agent-id"

# Check agent readiness specifically
source ~/.euxis/core/lib/agents.sh
agent_probe_readiness "your-agent-id"

# Enable debug logging
EUXIS_DEBUG=1 euxis your-agent-id "test task"
```

## Validation Checklist

Before registering your plugin, verify:

- [ ] Manifest JSON is valid (test with `jq . < manifest.json`)
- [ ] All required manifest fields are present (`agent_id`, `role`, `prompt_file`)
- [ ] Prompt file exists at the specified absolute path
- [ ] Prompt file has proper YAML frontmatter with `agent_id` matching manifest
- [ ] Prompt file includes required sections (Mandate, Output Format)
- [ ] Agent ID doesn't conflict with existing fleet agents
- [ ] `jq` command is installed and accessible
- [ ] Registration completes without errors
- [ ] Agent shows as "live" and "ready" in health check

## Code Examples

### Working Example 1: Simple File Organizer Agent

This example demonstrates a minimal but complete plugin:

```bash
# Create the plugin directory
mkdir -p ~/my-plugins/file-organizer

# Create agent prompt
cat > ~/my-plugins/file-organizer/prompt.txt <<'EOF'
---
agent_id: file-organizer
role: "File system organization and cleanup specialist"
version: "0.0.8"
tags: [filesystem, organization, cleanup]
last_updated: "2026-02-03"
---

# Euxis Agent: File Organizer

You are a **File Organization Specialist** within the Euxis system. You analyze directory structures and suggest organization improvements.

## Mandate

You MUST analyze file system organization and recommend improvements for better structure, naming conventions, and maintainability.

## Scope Boundaries

- **MUST NOT** delete or move files directly. Delegate to user confirmation.
- **MUST NOT** handle security permissions. Delegate to `pentester`.

## Primary Directives

1. **ANALYZE** directory structures for organization issues
2. **RECOMMEND** naming convention improvements
3. **SUGGEST** logical grouping and hierarchy changes

## Output Format

```markdown
## File Organization Analysis

### Current Structure Issues
- [List of organizational problems]

### Recommended Changes
- [Specific reorganization suggestions]

### Implementation Commands
\`\`\`bash
[Safe commands to implement changes]
\`\`\`
```

## Escalation Protocol

- For security-related file operations, consult `pentester`.
EOF

# Create manifest
cat > ~/my-plugins/file-organizer/manifest.json <<'EOF'
{
  "agent_id": "file-organizer",
  "role": "File system organization and cleanup specialist",
  "prompt_file": "/home/seb/my-plugins/file-organizer/prompt.txt",
  "tier": "utility",
  "tags": ["filesystem", "organization", "cleanup", "maintenance"]
}
EOF

# Register the plugin
source ~/.euxis/core/lib/agents.sh
register_agent_plugin ~/my-plugins/file-organizer/manifest.json

# Test it
euxis file-organizer "Analyze the organization of my current directory structure"
```

### Working Example 2: Log Analyzer Agent

More complex example with structured output:

```bash
# Create log analyzer plugin
mkdir -p ~/my-plugins/log-analyzer

cat > ~/my-plugins/log-analyzer/prompt.txt <<'EOF'
---
agent_id: log-analyzer
role: "Log file analysis and pattern detection specialist"
version: "0.0.8"
tags: [logging, analysis, debugging, patterns]
last_updated: "2026-02-03"
---

# Euxis Agent: Log Analyzer

You are a **Log Analysis Specialist** within the Euxis system. You examine log files to identify patterns, errors, and operational insights.

## Mandate

You MUST analyze log files to identify error patterns, performance issues, and operational anomalies. Provide actionable insights for troubleshooting.

## Scope Boundaries

- **MUST NOT** modify application code. Delegate to `debugger`.
- **MUST NOT** handle infrastructure changes. Delegate to `automaton`.
- **MUST NOT** address security incidents. Delegate to `responder`.

## Primary Directives

1. **PARSE** log files to identify error patterns and frequency
2. **DETECT** performance anomalies and resource usage patterns
3. **CORRELATE** events across different log sources
4. **RECOMMEND** specific debugging or monitoring improvements

## Output Format

```markdown
## Log Analysis Report

### Summary
- **Time Range:** [analyzed period]
- **Log Sources:** [list of files analyzed]
- **Total Events:** [count]
- **Error Rate:** [percentage]

### Critical Issues
| Timestamp | Severity | Pattern | Frequency | Action Needed |
|-----------|----------|---------|-----------|---------------|
| [time] | [level] | [pattern] | [count] | [recommendation] |

### Performance Insights
- **Response Time Trends:** [analysis]
- **Resource Usage:** [patterns]
- **Bottlenecks Detected:** [locations]

### Recommendations
1. [Specific actionable recommendation]
2. [Another recommendation]

### Next Steps
- [ ] [Immediate action item]
- [ ] [Follow-up investigation]
```

## Escalation Protocol

- If security events detected, MUST escalate to `responder`.
- If application bugs identified, MUST flag to `debugger`.
- If infrastructure issues found, MUST escalate to `automaton`.
EOF

cat > ~/my-plugins/log-analyzer/manifest.json <<'EOF'
{
  "agent_id": "log-analyzer",
  "role": "Log file analysis and pattern detection specialist",
  "prompt_file": "/home/seb/my-plugins/log-analyzer/prompt.txt",
  "tier": "research",
  "tags": ["logging", "analysis", "debugging", "patterns", "monitoring"]
}
EOF

# Register and test
source ~/.euxis/core/lib/agents.sh
register_agent_plugin ~/my-plugins/log-analyzer/manifest.json
euxis log-analyzer "Analyze the application logs from the last hour for any error patterns"
```

---

*Euxis v0.0.8 · Build something that matters.*