# Agent Loop Demo

Drives the libs/runtime AgentLoopHarness through a five-turn loop with a
bounded iteration budget, a libs/core CredentialPool that rotates on a
simulated 429, and a libs/metrics rollup producing a real per-session
cost estimate. Mock provider; no real LLM call.

Companion to [`a2a_minimal_server/`](../a2a_minimal_server/) — that one
exercises the A2A wire surface, this one exercises the agent-loop
surface.

## What it demonstrates

| Step | API surface |
|---|---|
| Build a credential pool | `euxis::core::CredentialPool`, `Credential` |
| Build a harness with budget + context window | `euxis::runtime::AgentLoopHarness`, `Config` |
| Run N turns with a mock provider | `harness.run_turn(fn)` |
| Classify a 429 and rotate credentials | `core::classify_failure()`, `recovery_for()`, `CredentialPool::cool_down()` |
| Track usage per turn | `metrics::UsageRecord`, `estimate_cost()`, `lookup_pricing()` |
| Aggregate session insights | `metrics::aggregate()` returning `SessionInsights` |
| Emit a JSON report | `nlohmann::json` |

## Build

```bash
cmake -B cmake-build -DEUXIS_BUILD_EXAMPLES=ON
cmake --build cmake-build --target euxis_example_agent_loop_demo
```

## Run

```bash
./cmake-build/docs/examples/cpp/agent_loop_demo/euxis_example_agent_loop_demo
```

## Expected output (excerpt)

```text
Credential pool: 3 keys, 3 currently available
Pricing for anthropic:claude-haiku-4-5 is known
Turn 2: simulated 429 — reason=rate_limit, action=retry; cooling key-c for 60s

[agent_loop_demo summary]
{
  "session_id": "demo-session-001",
  "total_calls": 5,
  "total_input_tokens": 1400,
  ...
  "total_cost_usd": 0.00295,
  "budget_remaining": 0,
  "credentials_available_now": 2,
  ...
}

Demo completed successfully.
```

The exact cost figure depends on the seeded pricing table in
`libs/metrics/src/insights.cpp` (current as of the cutoff documented
there).
