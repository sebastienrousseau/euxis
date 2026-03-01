# euxis-core-cpp

C++23 core execution engine with resilience primitives and cost-aware routing.

## Overview

Provides retry policies with exponential backoff and jitter, circuit breaker pattern, cost/speed/reliability-balanced model provider routing (FinOps), and multi-agent swarm orchestration with playbook execution.

## Key Types

- `RetryPolicy` -- Configurable exponential backoff with jitter
- `CircuitBreaker` -- Failure threshold with recovery timeout
- `FinOpsRouter` -- Intelligent provider selection (ollama, groq, anthropic, openai)
- `SwarmOrchestrator` -- Multi-agent playbook execution with simulation mode
- `AgentExecutionRequest/Result` -- Core value objects

## Dependencies

- `euxis-crypto-cpp`
- `euxis-metrics-cpp`
- `nlohmann_json::nlohmann_json`
- `spdlog::spdlog`

## Build

Part of the euxis-cpp CMake project. Built via `euxis_add_library()` macro.
