# Euxis CLI C++

The `euxis-cli` is the command-line orchestrator and Terminal User Interface (TUI) for the Euxis Agent OS. It executes hardware-native swarms and provides a high-fidelity visual interface for monitoring fleet latency, cost, and health.

## Hardware-Native Execution

The CLI operates directly against the C++23 core libraries.

* **Precondition**: Requires a TTY environment with VT100/ANSI escape sequence support.
* **Postcondition**: Renders a zero-latency, thread-safe Terminal User Interface.

For maximum rendering speed, the TUI minimizes dynamic memory allocation, utilizing `std::string_view` for textual manipulation. Ensure the host environment has `COLORTERM=truecolor` set to enable the advanced RGB styling engine.

## Agent Orchestration

Dispatch goals to the fleet using the unified command syntax.

```bash
# Delegate an autonomous task to the orchestrator agent
euxis dispatch "Audit the networking layer for race conditions."
```

The CLI invokes the `SwarmOrchestrator`, which initiates an auction-based bid across the mesh. Observe the real-time resource negotiation directly in the TUI log panel.

## Platform Parity

The CLI leverages the `euxis::platform` interface to normalize OS-specific anomalies.

* **Type Erasure**: Hiding concrete implementations — The CLI interacts with abstract platform contracts.

Whether running on macOS, Linux, or WSL, the CLI transparently resolves clipboard hooks (`pbcopy` vs `xclip` vs `clip.exe`) and web endpoints without conditional pollution in the rendering loops.