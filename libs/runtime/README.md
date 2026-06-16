# Euxis Runtime C++

The `euxis::runtime` module governs the lifecycle, context memory, and execution state of autonomous agents. It enforces the "Zero-Copy Mentality" and ensures memory-safe state transitions throughout the Agent OS.

## Episodic Memory

The `ISessionStore` interface provides stateless access to historical agent sessions.

* **Precondition**: A valid `.msgp` binary file or memory map exists for the session ID.
* **Postcondition**: Returns a `std::vector<SessionMessage>` carrying every message in the requested session branch.

For long-horizon reasoning, use the `stream_episodes` method. The orchestrator consumes the returned vector and may apply its own windowing / truncation before passing the slice to the LLM.

```cpp
auto messages = store.stream_episodes("session-1");
for (const auto& msg : messages) {
    process(msg);
}
```

### Future restoration to lazy streaming

The original `stream_episodes` signature returned `std::generator<SessionMessage>` (C++23) so a long history could be streamed without materialising the whole vector. AppleClang 21's libc++ and Homebrew LLVM 22's libc++ do not yet ship `<generator>`, so the signature was changed to eager `std::vector` and the implementation rewritten without `co_yield`. Restore the lazy form when `__cpp_lib_generator` is defined on the supported toolchains — the implementations already produce the messages in order and only need their `co_yield` body re-added.

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