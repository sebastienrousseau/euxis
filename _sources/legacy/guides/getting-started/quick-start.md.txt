# Quick Start Tutorial

Launch your first autonomous WebAssembly agent in less than 60 seconds.

This guide explores invoking a single agent natively from your terminal.

## 1. Initializing the Fleet Space

First, change your directory to the location where you want Euxis to execute tasks. Euxis strictly contains modifications to your current working directory.

```bash
mkdir my-euxis-project
cd my-euxis-project
```

## 2. Invoking the Architect

We will execute the `architect` agent. Provide your intent as a natural language directive using the `euxis` executable.

```bash
euxis architect "Initialize a standard python package struct including a pyproject.toml and a src/ directory."
```

## 3. Reviewing the Activity

Euxis instantly translates your command, spins up the WebAssembly context, and delegates the logic. The output trace will confirm:

1. **Resolution:** Extism container instantiated successfully.
2. **Execution:** The execution handler parsed the objective.
3. **Artifact Production:** The script generated the requested system structures.

Use `ls` to verify the creation of your `src/` directory and `pyproject.toml` file.

## What's Next?

You just executed a singular task. Production workloads utilize federated fleets of agents operating contextually across languages. Proceed to the [Environment Configuration](configuration.md) manual.
