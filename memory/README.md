# Memory

Memory owns the Cortex semantic storage layer and its on-disk data layout.

## Purpose
This module defines where Cortex data lives on disk and how memory persistence is organized. It is responsible for the Cortex storage directory structure and the canonical graph file. It does not implement the CLI interface or agent orchestration logic.

## Structure
- `cortex/` — Cortex data root (graph and database files)
- `cortex/graph.json` — Knowledge graph snapshot
- `cortex/db/` — Vector database storage (runtime data)

## Dependencies
- `core/` — shared utilities

## Usage
Memory is used by `cli/bin/euxis-cortex` and `cli/bin/euxis-graph` to read and write Cortex data.

## Development
```bash
# Validate cortex data paths
ls -la memory/cortex

# Inspect graph data
cat memory/cortex/graph.json
```

## Testing
Memory is exercised via `euxis-cortex` CLI tests and TUI Cortex screen tests.

## API / Exports
This module does not export code. It provides the on-disk Cortex data layout.
