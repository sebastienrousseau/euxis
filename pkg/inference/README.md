# euxis-inference-cpp

C++23 local inference engine — llama.cpp integration, Ollama client, model registry, quality gate.

## Overview

euxis-inference-cpp provides a local inference fallback for Euxis agents. It defines an InferenceEngine interface with two backends: LlamaEngine (a PIMPL-wrapped llama.cpp integration) and OllamaEngine (an HTTP client for the Ollama API). A model registry discovers GGUF files on disk and verifies their SHA-256 checksums. A quality gate scores inference outputs on coherence, relevance, and repetition before returning results.

## Dependencies

- libsodium
- nlohmann-json
- spdlog
- cpp-httplib

## Building

```bash
# From the euxis-cpp root
cmake -B build -S .
cmake --build build --target euxis-inference-cpp
```

## Testing

```bash
ctest --test-dir build -R euxis-inference-cpp_tests
```

## API

- **engine.hpp** -- InferenceEngine abstract interface (generate, embed, health).
- **config.hpp** -- InferenceConfig for model parameters, temperature, max tokens.
- **model_registry.hpp** -- GGUF model discovery, SHA-256 integrity verification, model listing.
- **quality_gate.hpp** -- Output quality scoring (coherence, relevance, repetition) and threshold enforcement.
- **llama_engine.hpp** -- LlamaEngine: PIMPL llama.cpp backend.
- **ollama_engine.hpp** -- OllamaEngine: HTTP client for the Ollama REST API.
