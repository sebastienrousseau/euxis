# Coroutine-based Streaming Provider

This example shows how to consume a `std::generator<ProviderDelta>` from an `IStreamingProvider`: accumulate text fragments into one assistant message, reassemble tool-call argument JSON keyed by id, and capture the final usage record.

## What this example shows

Real LLM SSE streams emit small, interleaved chunks — partial text and partial tool-call JSON arrive in any order, scattered across many events. The runtime's `IStreamingProvider` interface returns a coroutine-friendly generator, and the consumer walks it with a normal range-for. This example is the canonical pattern: copy the `drain_stream` helper into your own runtime when you wire a real provider.

## Why the API is coroutine-based

A `std::generator<T>` lets each delta be processed lazily, exactly once, in the order the wire produced it. The caller never has to allocate an intermediate vector or set up a callback chain. When the stream ends, the generator's natural end-of-range terminates the for-loop — no special signal, no exception path.

## Toolchain availability

`<generator>` (C++23, P2502) ships in libstdc++ 14 but not yet in libc++ as of 2026-06. The `IStreamingProvider` declaration in `streaming.hpp` is therefore guarded by `EUXIS_HAS_STD_GENERATOR`. On toolchains that lack it (AppleClang 21, Homebrew LLVM 22) this binary compiles to a one-line "build skipped" message so the example tree builds cleanly everywhere; on GCC 14 the binary runs the full demo.

## Build and run

```bash
cmake -B cmake-build -DEUXIS_BUILD_EXAMPLES=ON
cmake --build cmake-build --target euxis_example_streaming_loop
./cmake-build/docs/examples/cpp/streaming_loop/euxis_example_streaming_loop
```

On a toolchain with `<generator>`, the binary streams an assistant turn live to stdout, prints the assembled tool-call arguments, and exits zero. On a toolchain without it, it prints the skip notice and still exits zero.
