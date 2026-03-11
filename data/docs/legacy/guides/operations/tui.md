# Terminal User Interface (TUI)

The Euxis TUI connects natively to the Euxis Gateway using high-throughput websockets. It visualizes concurrent memory allocation, agent telemetry, and system faults directly within your standard terminal emulator.

It completely abandons archaic synchronous logging and instead implements modern "Liquid Glass" rendering patterns natively.

## Bootstrapping the Dashboard

Access the master overview interface utilizing the interactive command:

```bash
euxis ui
```

## Navigating the Interface

The interface operates through native keyboard-driven widgets configured specifically to minimize context shifts.

### The Fleet Grid
The center module displays concurrent Extism agent containers. It maps memory allocation parameters against actual real-time consumption constraints to identify leaking modules.

### The Event Stream
The bottom-left module intercepts WebSocket log streams natively, classifying messages by their `DEBUG`, `INFO`, `WARN`, or `ERROR` headers.

### The Execution Payload
The top-right panel intercepts conversational intents between you and the orchestrated agent fleet.

## Hotkey Navigation References

| Keystroke | Action |
|---|---|
| `Tab` | Shift focus sequence between modules. |
| `/` | Bring focus to the interactive user intent submission textbox. |
| `Ctrl+C` | Gracefully suspend active Wasm execution vectors and detach from the local Gateway instance. |
| `c` | Interactively purge the event streams and clear local WebSocket caches. |

## Theme Configuration

The interface inherits its color palettes intelligently from your standard terminal environment variables. For specific customizations, overwrite the standard definitions internally by defining `EUXIS_THEME=nord` or `EUXIS_THEME=dracula` within your `.bashrc` or equivalent profile initialization schema.
