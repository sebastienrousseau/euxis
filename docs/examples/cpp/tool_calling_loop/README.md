# Closed Tool-Calling Agent Loop

This example shows how to close the Euxis agent loop end-to-end: parse tool calls from a model response, classify them by approval band, gate them through an operator callback, dispatch them via `IToolRegistry`, and re-feed the results into the next turn.

## What you get out of this example

You get the exact pattern Hermes-style and OpenClaw-style agent runtimes ship out of the box. The runtime library deliberately stops at the provider response — `AgentLoopHarness::run_turn` returns a `ProviderResponse` and lets you decide what to do next. This example fills that decision in with one free function, `dispatch_tool_calls`, that you can copy verbatim into your own runtime.

## Why the loop lives in the caller

`AgentLoopHarness` is provider-agnostic and tool-agnostic on purpose. The harness owns the iteration budget, the context-engine query, and the conversation vector. It does not own a JSON dialect for tool calls because every model vendor encodes them differently. Keeping the dispatch out of the harness means you can swap Anthropic, OpenAI, Gemini, or a local Ollama backend without touching runtime internals.

## The approval gate

Every tool call passes through `classify_approval` from `tool_manifest.hpp`. The classifier returns one of three bands — `Readonly`, `ExecCapable`, or `ControlPlane` — based on the tool name and an optional required scope. The example installs a permissive `ApprovalCallback` that logs the class and approves everything; production callers should deny `ExecCapable` and `ControlPlane` by default and prompt the operator.

## Build and run

Configure with examples enabled and build the target.

```bash
cmake -B cmake-build -DEUXIS_BUILD_EXAMPLES=ON
cmake --build cmake-build --target euxis_example_tool_calling_loop
./cmake-build/docs/examples/cpp/tool_calling_loop/euxis_example_tool_calling_loop
```

The binary prints a per-turn decision trace, a final summary line, and exits zero when the mock provider emits its `final` message.
