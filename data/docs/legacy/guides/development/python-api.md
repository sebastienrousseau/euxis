# Using the Native Python API

You can embed the `euxis-core` Engine natively inside your Python workloads. This bypasses the CLI router and communicates seamlessly with the WebAssembly mesh natively using asynchronous execution mappings.

## Installation Requirement

Ensure you have installed the native engine.

```bash
pip install euxis-core
```

## 1. Instantiating the Engine

Instantiate the framework synchronously or asynchronously. The Engine handles local WebAssembly memory initialization implicitly.

```python
from euxis_core import Engine

engine = Engine(log_level="info")

# Verify native Extism connectivity
engine.status()
```

## 2. Synchronous Execution

Execute specific agents sequentially. This approach explicitly maps natural language intent into the selected module natively.

```python
response = engine.dispatch(
    agent="architect",
    intent="Map out an enterprise authentication component using JWTs."
)

print(response.artifact)
```

## 3. Asynchronous Execution Graphs

To achieve massive concurrency throughput, use the standard `asyncio` loop arrays to launch hundreds of native Wasm evaluations simultaneously natively.

```python
import asyncio
from euxis_core import AsyncEngine

async def run_fleet():
    engine = AsyncEngine()
    
    # Execute 3 separate agent evaluations flawlessly using native concurrency
    tasks = [
        engine.dispatch(agent="tester", intent="Validate frontend components."),
        engine.dispatch(agent="tester", intent="Validate backend API endpoints."),
        engine.dispatch(agent="auditor", intent="Scan structural complexity natively.")
    ]
    
    results = await asyncio.gather(*tasks)
    
    for r in results:
        print(r.status, r.duration_ms)

asyncio.run(run_fleet())
```

## Handling Execution Traps

The `Engine` wraps specific Extism exceptions internally. Wrap your `dispatch` calls in explicit `try/except` parameters to handle authentication or validation failures natively.

```python
from euxis_core.exceptions import SandboxViolationError, GatewayTimeoutError

try:
    engine.dispatch(agent="untrusted_agent", intent="Format my hard drive")
except SandboxViolationError as e:
    print("Execution blocked by Gateway hypervisor.", str(e))
```
