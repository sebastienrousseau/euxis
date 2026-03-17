# Euxis Inference C++

The `euxis::inference` module bridges the Agent OS cognitive layer with underlying Small Language Models (SLMs) and hardware accelerators. It provides a universal, C++23 interface for model generation, stream decoding, and quality enforcement.

## Extism WASM Sandboxing

Euxis delegates strategic reasoning to sandboxed WebAssembly (WASM) agents using Extism. 

* **AOT**: Ahead-Of-Time compilation — Pre-compiled binary logic.
* **Precondition**: The host environment must expose the `euxis-mcp` host functions.

For native speed, utilize the Ahead-Of-Time (AOT) pipeline. This eliminates the JIT overhead associated with dynamic instantiation, dropping "Time-to-First-Action" below the 10ms threshold.

## Local Model Orchestration

The `LlamaEngine` and `OllamaEngine` implementations abstract the underlying completion protocols. 

* **Postcondition**: Return a monadic `std::expected` resolving to an `InferenceResult`.
* **Zero-Copy**: Map binary context directly without intermediate strings.

To execute long-horizon reasoning tasks without blowing out VRAM, always use the `episodic_generate` method. This consumes a `std::generator<SessionMessage>` to stream semantic context lazily.

## Monadic Quality Gates

Every inference payload must pass the `QualityGate` before execution or delegation. 

```cpp
auto valid_plan = engine.generate(prompt)
    .and_then([](auto&& res) { return validate_schema(res.text); })
    .or_else([](auto&& err) { return fallback_reasoning(err); });
```

The system uses C++23 monadic operations to avoid `try-catch` branch penalties in the hot path.