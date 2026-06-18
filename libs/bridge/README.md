<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::bridge</h1>

<p align="center">
  Foreign-skill ingestion for euxis: imports OpenClaw / ClawHub
  <code>SKILL.md</code> bundles, runs static analysis on them, admits them
  against a policy, executes them sandboxed, and signs their provenance.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Pipeline at a glance](#pipeline-at-a-glance)
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/bridge)
target_link_libraries(my_app PRIVATE euxis-bridge-cpp)
```

## Pipeline at a glance

```
SKILL.md (ClawHub bundle)
    │
    ▼
ClawHubImporter ── parse frontmatter ──▶ BridgedSkill
    │
    ▼
SkillStaticAnalyzer ──▶ AnalysisReport (findings, severity)
    │
    ▼
AdmissionPipeline ── checks policy + reputation ──▶ AdmissionResult
    │ (admitted)
    ▼
SkillExecutor ── sandboxed exec via libs/platform ──▶ ExecutionResult
    │
    ▼
AuditLogger ── stream events ──▶ ProvenanceChain (signed)
```

## Public surface

| Header | What it owns |
|---|---|
| `parser.hpp` | `Frontmatter` parser for `SKILL.md` |
| `skill.hpp` | `BridgedSkill` — the canonical imported skill record |
| `importer.hpp` | `ClawHubImporter` — fetches and parses ClawHub skill bundles |
| `static_analysis.hpp` | `SkillStaticAnalyzer`, `AnalysisFinding`, `AnalysisReport`, `Severity` |
| `policy.hpp` | `ResourceLimits`, `FilesystemPolicy`, `NetworkPolicy`, `SkillExecutionPolicy`, `AgentCapabilityToken` |
| `admission.hpp` | `AdmissionPipeline` — policy + reputation + static-analysis gate; returns `AdmissionResult` |
| `executor.hpp` | `SkillExecutor` — runs admitted skills via `libs/platform` execution backends; returns `ExecutionResult` |
| `audit.hpp` | `AuditLogger` — streams execution events |
| `provenance.hpp` | `ProvenanceEntry`, `ProvenanceChain` — signed chain of skill imports + executions |
| `reputation.hpp` | `AuthorReputation`, `ReputationStore` — tracks per-author trust signals |
| `verification.hpp` | `VerificationResult` — output of cryptographic skill-bundle verification |
| `platform.hpp` | `PlatformInfo` — environment probe consumed by the policy stage |

## Examples

### Full ingest → admit → execute round-trip

```cpp
#include "euxis/bridge/importer.hpp"
#include "euxis/bridge/admission.hpp"
#include "euxis/bridge/executor.hpp"

using namespace euxis::bridge;

ClawHubImporter importer;
auto bundle = importer.import_local("./vendor-skill.md");
if (!bundle) { /* handle parse failure */ }

AdmissionPipeline admission{/*default policy*/};
auto verdict = admission.evaluate(*bundle);
if (verdict.admitted) {
    SkillExecutor exec;
    const auto out = exec.run(*bundle, /*input=*/"audit src/");
}
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
