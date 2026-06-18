<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::security</h1>

<p align="center">
  The canonical Finding type for euxis plus the policy and governance
  shapes consumed across the verification stack. C++23 value types only —
  no I/O, no LLM calls.
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
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/security)
target_link_libraries(my_app PRIVATE euxis-security-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/security/finding.hpp"

int main() {
    using namespace euxis::security;

    Finding f;
    f.stable_fingerprint = "fp-001";
    f.rule_id            = "euxis/cwe-79";
    f.message            = "XSS sink reachable from untrusted input";
    f.severity           = Severity::High;
    f.confidence         = Confidence::Probable;
    f.source             = RuleSource::EuxisCore;
    f.primary_location.path         = "src/handlers/echo.cpp";
    f.primary_location.start_line   = 42;
    f.primary_location.start_column = 7;
    f.cwe.push_back(CweRef{.id = 79, .name = "Improper Neutralization of Input"});

    std::cout << "finding emitted: " << f.rule_id
              << " @ " << f.primary_location.path
              << ":"   << f.primary_location.start_line << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `finding.hpp` | `Finding` (the canonical scan-result record), `Severity`, `Confidence`, `RuleSource`, `CweRef`, `OwaspCategory`, `QuantumDeprecation`, `SourceLocation`, `FixSuggestion`, `Finding::EnsembleVote` |
| `policy.hpp` | `SecurityPolicy` — allowlist, severity floor, scope rules |
| `governance.hpp` | `GovernanceCheck`, `GovernanceViolation`, `GovernanceResult`, `GovernancePolicy` |
| `errors.hpp` | `PolicyError` exception type |

This module is values-only. Every scanner (`libs/scan`, `libs/taint`, `libs/reach`) emits `Finding`s; the ensemble runner (`libs/ensemble`) attaches `EnsembleVote`s to them; the SARIF and SBOM emitters consume them.

## Examples

### Filter findings against a severity floor

```cpp
#include "euxis/security/policy.hpp"

SecurityPolicy policy;
policy.minimum_severity = Severity::Medium;

std::erase_if(findings, [&](const Finding& f) {
    return static_cast<int>(f.severity) < static_cast<int>(policy.minimum_severity);
});
```

### Run a governance check against a snapshot

```cpp
#include "euxis/security/governance.hpp"

GovernancePolicy gp;
gp.checks = {GovernanceCheck::SignedCommits, GovernanceCheck::BranchProtection};

auto result = evaluate_governance(repo_snapshot, gp);
if (!result.violations.empty()) { /* enforce */ }
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
