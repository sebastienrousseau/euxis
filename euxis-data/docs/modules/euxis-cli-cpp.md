# euxis-cli-cpp

C++23 command-line interface for the Euxis agent framework.

## Overview

Executable CLI providing command dispatch (version, help, bridge, gateway), OmniGraph formatting for multiple AI providers, and token budget optimization with graph pruning.

## Key Types

- `Engine` -- Command dispatch engine
- `GraphAdapter` -- Multi-provider graph formatting (Claude, OpenAI, Ollama)
- `TokenBudgeter` -- Token estimation and graph size optimization

## Key Functions

- `Engine::run(args)` -- Main command dispatch
- `GraphAdapter::format_for_provider(graph, provider)` -- Provider-specific formatting
- `TokenBudgeter::optimize_graph(graph, format)` -- Budget-aware graph pruning

## Dependencies

Links: `euxis-core-cpp`, `euxis-gateway-cpp`, `euxis-metrics-cpp`, `euxis-security-cpp`

## Build

Built as `add_executable(euxis-cli)`. Part of the euxis-cpp CMake project.
