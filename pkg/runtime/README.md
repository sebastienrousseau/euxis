# Euxis Runtime C++

The `euxis::runtime` module governs the lifecycle, context memory, and execution state of autonomous agents. It enforces the "Zero-Copy Mentality" and ensures memory-safe state transitions throughout the Agent OS.

## Episodic Memory Streaming

The `ISessionStore` interface provides high-performance, stateless access to historical agent sessions.

* **Precondition**: A valid `.msgp` binary filter or memory map exists for the session ID.
* **Postcondition**: Returns a C++23 `std::generator` yielding `SessionMessage` objects.

For long-horizon reasoning, use the `stream_episodes` method. This allows the orchestrator to lazy-load semantic traces without blowing out the host's VRAM or the LLM's context window.

```cpp
auto stream = store.stream_episodes("session-1");
for (auto msg : stream) {
    process(msg); // Lazy evaluation
}
```

## Binary Serialization

Euxis avoids the overhead of JSON parsing in the hot path. The runtime layer exclusively serializes agent state to MessagePack.

* **Zero-Copy**: Map binary snapshots directly into memory via `mmap`.
* **std::span**: Bounds-checked memory view — Use for safe contiguous access to binary payloads.

## Monadic Error Chaining

Handle execution faults using C++23 `std::expected`. Avoid exception-heavy try/catch blocks in standard logic flows.

* **Monadic**: Functional error chaining.

```cpp
auto res = store.load("session")
    .and_then([](auto&& snap) { return validate(snap); })
    .or_else([](auto&& err) { return fallback(err); });
```

**All components must achieve 100% test coverage for success and failure branches.**