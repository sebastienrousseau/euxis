<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::ensemble</h1>

<p align="center">
  Verifier-quorum runner for euxis: multi-LLM voting on
  <code>euxis::security::Finding</code> objects with optional
  unanimity, plus real Claude / OpenAI / Gemini HTTP verifiers.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start)
- [Public surface](#public-surface)
- [Examples](#examples)
- [Performance](#performance)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/ensemble)
target_link_libraries(my_app PRIVATE euxis-ensemble-cpp)
```

## Quick start

```cpp
#include <iostream>
#include <memory>
#include <vector>

#include "euxis/ensemble/runner.hpp"
#include "euxis/ensemble/providers/claude.hpp"
#include "euxis/security/finding.hpp"

int main() {
    using namespace euxis::ensemble;
    using namespace euxis::security;

    // 1. Build findings from your scanner pass.
    std::vector<Finding> findings = /* from libs/scan, libs/taint, etc. */;

    // 2. Assemble verifiers. Each one votes on every finding.
    std::vector<VerifierPtr> verifiers{
        std::make_shared<providers::ClaudeVerifier>(),
        // std::make_shared<providers::OpenAIVerifier>(),
        // std::make_shared<providers::GeminiVerifier>(),
    };

    // 3. Run the quorum. Quorum=2 + 3 verifiers ≈ majority vote.
    EnsembleConfig cfg;
    cfg.quorum            = 2;
    cfg.require_unanimous = false;
    cfg.max_snippet_bytes = 4096;

    const auto result = verify(std::move(findings), verifiers, cfg);
    std::cout << "confirmed: " << result.stats.confirmed
              << "  rejected: " << result.stats.rejected << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `runner.hpp` | `verify(findings, verifiers, config)` — the quorum runner; `EnsembleConfig`, `EnsembleResult`, `EnsembleStats` |
| `verifier.hpp` | `Verifier` (abstract), `VoteRequest`, `VoteOutcome`, `VerifierPtr` |
| `prompt.hpp` | `build_prompt(req, config)` — shared prompt synthesis |
| `response.hpp` | `parse_response(text)` — extracts the `{true_positive, confidence}` tuple from a verifier reply |
| `deterministic.hpp` | `DeterministicVerifier` — votes by static rule, useful for tests and fast-path screening |
| `providers/http_verifier.hpp` | `HttpVerifier` base + `HttpVerifierConfig` (api_key, env_var_name, timeout, retries) |
| `providers/claude.hpp` | `ClaudeVerifier` — POSTs to Anthropic `/v1/messages` |
| `providers/openai.hpp` | `OpenAIVerifier` — POSTs to OpenAI `/v1/chat/completions` |
| `providers/gemini.hpp` | `GeminiVerifier` — POSTs to Gemini `generateContent` |

## Examples

### Deterministic-only run (no LLM calls, useful in CI)

```cpp
#include "euxis/ensemble/deterministic.hpp"

auto verifiers = std::vector<VerifierPtr>{
    std::make_shared<DeterministicVerifier>(/*always=*/true),
};
const auto out = verify(std::move(findings), verifiers, {.quorum = 1});
```

### Require unanimity for high-confidence verdicts

```cpp
EnsembleConfig cfg;
cfg.quorum            = 3;
cfg.require_unanimous = true;   // every verifier must agree
const auto out = verify(std::move(findings), verifiers, cfg);
```

A finding only flips to `Confidence::Verified` when every active verifier votes `true_positive`. Failing the unanimity bar downgrades to `Probable` (when the quorum is met by majority) or `Possible` (when below the quorum).

## Performance

Measured on Apple M1 with `EUXIS_BUILD_GBENCH=ON`:

| Bench | Throughput |
|---|---|
| `BM_Ensemble_Verify_1Verifier/1024`   | ~17 M findings/s |
| `BM_Ensemble_Verify_2Verifiers/1024`  | ~12 M findings/s |
| `BM_Ensemble_Verify_3Verifiers/1024`  | ~8 M findings/s  |

These numbers exercise the runner with a `DeterministicVerifier`-style mock — real LLM votes are network-bound.

## Testing

```bash
cmake --build build/cmake-build --target euxis-ensemble-cpp_tests
./build/cmake-build/libs/ensemble/euxis-ensemble-cpp_tests
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
