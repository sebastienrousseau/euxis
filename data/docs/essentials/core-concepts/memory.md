# Memory

## What It Is

Memory in Euxis is a typed knowledge system that enables agents to learn, remember, and improve across sessions. It combines vector-based semantic search (Cortex) with structured knowledge graphs to create persistent, queryable intelligence.

## When to Use

- **Cross-session learning**: Remember successful patterns and failed approaches
- **Knowledge accumulation**: Build domain expertise over time
- **Context recall**: Retrieve relevant past experiences for current tasks
- **Pattern recognition**: Identify repeated issues and proven solutions

## Quick Example

```bash
# Store successful workflow
euxis-cortex remember "API testing with Postman requires auth token setup first" "inspector" --type procedural

# Store project fact
euxis-cortex remember "User service uses PostgreSQL 14 with connection pooling" "architect" --type semantic

# Store task outcome
euxis-cortex remember "Fixed OAuth2 timeout by increasing token expiry to 3600s" "debugger" --type episodic

# Recall relevant knowledge
euxis-cortex recall "OAuth2 debugging" --type procedural
euxis-cortex recall "authentication issues" 5
```

## Memory Types

### Episodic Memory
**What**: Specific events, outcomes, and interactions from sessions
**When**: Store completed tasks, bug fixes, successful deployments
**Format**: "What happened, when, result"

```bash
# Examples of episodic memories
euxis-cortex remember "Deployed v2.1.0 to production, all health checks passed" "gatekeeper" --type episodic
euxis-cortex remember "Fixed null pointer in auth.py line 89 by adding guard clause" "debugger" --type episodic
euxis-cortex remember "Performance testing revealed 50ms latency improvement after database indexing" "optimizer" --type episodic
```

### Semantic Memory
**What**: General facts, rules, and relationships that persist across sessions
**When**: Store architectural decisions, API contracts, configuration rules
**Format**: "General knowledge about systems, relationships, constraints"

```bash
# Examples of semantic memories
euxis-cortex remember "The auth module uses JWT tokens with RS256 signing" "architect" --type semantic
euxis-cortex remember "Database migrations must be backward compatible" "maintainer" --type semantic
euxis-cortex remember "API rate limit is 1000 requests per minute per user" "planner" --type semantic
```

### Procedural Memory
**What**: Reusable workflows, patterns, and multi-step recipes
**When**: Store proven approaches, debugging strategies, contraindications
**Format**: "How to do X: step 1 → step 2 → step 3"

```bash
# Examples of procedural memories
euxis-cortex remember "To debug auth failures: check token expiry → verify signing key → inspect claims" "debugger" --type procedural
euxis-cortex remember "CONTRAINDICATION: Do NOT retry auth with expired refresh tokens: always re-authenticate" "debugger" --type procedural
euxis-cortex remember "To deploy: run tests → build → tag → push → verify health" "automaton" --type procedural
```

## Memory Operations

### Storing Memories

```bash
# Store with required parameters
euxis-cortex remember "<fact or pattern>" "<agent-id>" --type <episodic|semantic|procedural>

# Examples
euxis-cortex remember "Redis cache timeout set to 300s for session data" "telemetrist" --type semantic
euxis-cortex remember "Load testing revealed memory leak in WebSocket connections" "inspector" --type episodic
```

### Retrieving Memories

```bash
# Semantic search across all memories
euxis-cortex recall "<query>" [n]

# Type-filtered search
euxis-cortex recall "<query>" --type procedural
euxis-cortex recall "<query>" --type episodic
euxis-cortex recall "<query>" --type semantic

# Limited results
euxis-cortex recall "authentication" 3
```

### Memory Management

```bash
# View memory statistics
euxis-cortex stats

# Remove specific memory
euxis-cortex forget "<exact text to remove>"

# Query memory patterns
euxis-cortex recall "CONTRAINDICATION" --type procedural
```

## Agent Memory Integration

### Pre-Task Recall (Mandatory)
Before complex tasks, agents MUST recall relevant memories:

```bash
# Agent automatically recalls procedural patterns
euxis-cortex recall "<task keywords>" --type procedural

# Cross-reference with episodic outcomes
euxis-cortex recall "<task keywords>" --type episodic
```

### Post-Task Storage (Mandatory)
After task completion, agents MUST store outcomes:

```bash
# Successful patterns
euxis-cortex remember "Pattern: X leads to Y with Z constraints" "agent-id" --type procedural

# Failed approaches (contraindications)
euxis-cortex remember "CONTRAINDICATION: Approach X fails because Y. Use Z instead." "agent-id" --type procedural

# Specific outcomes
euxis-cortex remember "Task X completed with outcome Y on date Z" "agent-id" --type episodic
```

## Knowledge Graph Integration

### Entity Relationships
```bash
# Create connected concepts (if available)
euxis-graph add-edge "authentication" "relates-to" "jwt-tokens"
euxis-graph add-edge "performance" "depends-on" "database-indexing"

# Query relationships
euxis-graph neighbors "authentication"
```

### Hybrid Recall
Combine vector search with graph traversal for comprehensive context.

## Memory Patterns

### Successful Workflows
Store proven multi-step processes:
```
"To implement OAuth2: configure provider → set up endpoints → add middleware → test token flow"
```

### Contraindications
Mark failed approaches to prevent repetition:
```
"CONTRAINDICATION: Do NOT use synchronous database calls in async routes: causes blocking"
```

### System Knowledge
Capture architectural decisions and constraints:
```
"API uses microservices with Redis for cross-service session sharing"
```

### Performance Baselines
Remember optimization benchmarks:
```
"Database query optimization improved response time from 200ms to 50ms"
```

## Create Your Own Memory Strategy

### Team Memory Sharing

```bash
# Development team patterns
euxis-cortex remember "Code review checklist: security → performance → readability → tests" "reviewer" --type procedural

# Operations patterns
euxis-cortex remember "Deployment rollback: stop traffic → revert → verify health → restore traffic" "responder" --type procedural

# Quality patterns
euxis-cortex remember "Performance testing: baseline → load test → stress test → soak test" "inspector" --type procedural
```

### Domain-Specific Knowledge

```bash
# Security domain
euxis-cortex remember "Authentication flows must validate: signature → expiry → audience → scope" "pentester" --type semantic

# Payment domain
euxis-cortex remember "PCI compliance requires: data encryption → access logging → secure transmission" "auditor" --type semantic
```

## Memory Best Practices

### Granularity
- **One concept per memory**: Store focused, atomic facts
- **Clear attribution**: Always include agent source
- **Specific context**: Include enough detail for future relevance

### Naming Conventions
- **Actions**: "To do X: steps..."
- **Contraindications**: "CONTRAINDICATION: Never do X because Y"
- **Facts**: "System X uses Y with constraint Z"
- **Outcomes**: "Task X resulted in Y on date Z"

### Memory Hygiene
- **Review patterns**: Periodically query for outdated knowledge
- **Update facts**: Replace superseded information
- **Prune duplicates**: Remove redundant or conflicting memories

## Available Options

```bash
# Basic operations
euxis-cortex remember "<fact>" "<agent>" --type <type>
euxis-cortex recall "<query>" [n] [--type <type>]
euxis-cortex forget "<text>"
euxis-cortex stats

# Advanced querying
euxis-cortex recall "performance optimization" --type procedural
euxis-cortex recall "authentication" 5
euxis-cortex recall "CONTRAINDICATION" --type procedural
```

**See Also**: [Agents](agents.md) for memory integration patterns, [Protocol](../CONSTITUTION.md) for mandatory memory discipline.