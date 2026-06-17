# Euxis Runtime C++

The `euxis::runtime` module governs the lifecycle, context memory, and execution state of autonomous agents. It enforces the "Zero-Copy Mentality" and ensures memory-safe state transitions throughout the Agent OS.

## Episodic Memory

The `ISessionStore` interface provides stateless access to historical agent sessions.

* **Precondition**: A valid `.msgp` binary file or memory map exists for the session ID.
* **Postcondition**: Returns `std::generator<SessionMessage>` on toolchains that ship `<generator>` (GCC 14's libstdc++), or `std::vector<SessionMessage>` as a fallback on toolchains that don't (AppleClang 21 / Homebrew LLVM 22 libc++ as of 2026-06). Range-for at the call site is identical either way.

For long-horizon reasoning, use the `stream_episodes` method. On the lazy path the implementation `co_yield`s one message at a time so a multi-thousand-message branch does not have to materialise into a vector before iteration begins.

```cpp
for (auto&& msg : store.stream_episodes("session-1")) {
    process(msg);
}
```

### Lazy vs eager — picked at compile time

The header signature flips behind `EUXIS_HAS_STD_GENERATOR`, set by `libs/runtime/include/euxis/runtime/streaming.hpp` when `__cpp_lib_generator >= 202207L` is defined. When a toolchain catches up to the standard, no call site has to move — only the guard.

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