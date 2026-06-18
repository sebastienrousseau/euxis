<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::inference</h1>

<p align="center">
  LLM backends for euxis: local engines (llama.cpp, Ollama), HTTP-streaming
  providers (Anthropic Claude, OpenAI, Gemini) implementing
  <code>IStreamingProvider</code>, a model registry, and Anthropic
  prompt-cache discipline helpers.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — stream a Claude response via SSE
- [Public surface](#public-surface)
- [Examples](#examples) — local + remote + cache-control
- [Toolchain notes](#toolchain-notes)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/inference)
target_link_libraries(my_app PRIVATE euxis-inference-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/inference/claude_streaming.hpp"

int main() {
    using namespace euxis::inference;
    using euxis::runtime::DeltaKind;

    ClaudeStreamingConfig cfg;                              // reads ANTHROPIC_API_KEY by default
    cfg.model = "claude-sonnet-4-6";

    ClaudeStreamingProvider provider{cfg};

    std::string assembled;
    for (auto&& d : provider.execute_stream("claude-sonnet-4-6", "Summarise the auth module.")) {
        if (d.kind == DeltaKind::TextFragment) {
            assembled += d.text;
            std::cout << d.text << std::flush;
        }
    }
    std::cout << "\n(assembled " << assembled.size() << " bytes)\n";
}
```

Requires a toolchain with `<generator>` — see [Toolchain notes](#toolchain-notes).

## Public surface

| Header | What it owns |
|---|---|
| `engine.hpp` | `IInferenceEngine` — minimal `generate(model, prompt, params) -> std::string` interface |
| `llama_engine.hpp` | `LlamaEngine`, plus the older `InferenceEngine` base and `InferenceResult` shape |
| `ollama_engine.hpp` | `OllamaEngine` — talks to a local Ollama daemon over HTTP |
| `claude_streaming.hpp` | `ClaudeStreamingProvider` (`euxis::runtime::IStreamingProvider`), `ClaudeStreamingConfig` |
| `openai_streaming.hpp` | `OpenAIStreamingProvider`, `OpenAIStreamingConfig` |
| `gemini_streaming.hpp` | `GeminiStreamingProvider`, `GeminiStreamingConfig` |
| `cache_control.hpp` | Anthropic prompt-cache discipline — marker placement on last system + last user message; idempotent re-application |
| `quality_gate.hpp` | `QualityGate`, `QualityScore` — pre-execution quality scoring |
| `model_registry.hpp` | `ModelRegistry`, `ModelInfo` — discovery + metadata for available models |
| `config.hpp` | `LocalModelConfig` |

## Examples

### Local llama.cpp inference

```cpp
#include "euxis/inference/llama_engine.hpp"

LlamaEngine engine{/*model_path=*/"models/llama-3-8b-q4.gguf"};
const auto out = engine.generate("llama-3-8b", "Hi", /*params=*/{});
```

### Quality-gate a candidate response

```cpp
#include "euxis/inference/quality_gate.hpp"

QualityGate gate;
const auto score = gate.score(candidate_text, /*context=*/prompt);
if (score.quality < 0.7) reject();
```

### Anthropic prompt-cache markers

```cpp
#include "euxis/inference/cache_control.hpp"

auto messages = build_messages(...);
apply_cache_control(messages);                 // idempotent; safe to call repeatedly
```

## Toolchain notes

`IStreamingProvider` and the three streaming-provider classes are gated by `EUXIS_HAS_STD_GENERATOR` (set when `__cpp_lib_generator >= 202207L`). GCC 14's libstdc++ ships `<generator>`; Apple Clang 21 / Homebrew LLVM 22 libc++ does not as of 2026-06. On unsupported toolchains the streaming symbols compile out and the related tests skip cleanly.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
