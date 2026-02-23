# Deploying Squads

Squad deployments represent the multi-agent evolution. A singular agent excels at granular task resolution; squads resolve massive architectural directives encompassing design, development, and validation endpoints.

## The Federation Pattern

Euxis operates federated task queues natively in the Gateway memory blocks. 

1. You dispatch a supreme directive.
2. The specialized `orchestrator` agent parses the directive into an execution DAG (Directed Acyclic Graph).
3. The Gateway invokes specific Extism containers (`architect`, `tester`, `maintainer`) synchronously or concurrently based on the underlying DAG structure.

## Dispatching Squads

Dispatch complex refactoring operations using the fundamental `squad deploy` CLI structure.

```bash
euxis squad deploy quality "Audit the entire payment processing infrastructure and implement strict validation guards."
```

In this mode, Euxis translates the intent, spins up an isolated fleet, and streams progress indicators. Once complete, it generates an operational walk-through matrix detailing what components passed inspection and what the engineer agent modified.

## Defining Custom Topologies

Instead of defaulting to native `quality` or `research` heuristic mappings, you specify precise agent interconnections natively in YAML templates mapping the topology structure.

### Example: The Pipeline Topology

Store your custom configurations natively inside your `~/.euxis/squads/` directory.

```yaml
# ~/.euxis/squads/backend_pipeline.yaml
name: "Backend Verification Pipeline"
nodes:
  - id: analyze
    agent: architect
    task: "Extract domain bounds."
  - id: execute
    agent: maintainer
    task: "Implement boundaries."
    depends_on: [analyze]
  - id: verify
    agent: tester
    task: "Assert strictly 100% unit test coverage."
    depends_on: [execute]
```

Trigger your custom pipeline graph natively.

```bash
euxis squad run backend_pipeline
```
