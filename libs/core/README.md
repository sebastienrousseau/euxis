<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::core</h1>

<p align="center">
  Swarm orchestration primitives for euxis: capability router, supervisor,
  credential pool with cooldown, parallel subagent delegation. C++23.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — rotate a credential pool on a simulated 429
- [Public surface](#public-surface)
- [Examples](#examples)
- [Testing](#testing)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/core)
target_link_libraries(my_app PRIVATE euxis-core-cpp)
```

## Quick start

```cpp
#include <chrono>
#include <iostream>
#include <vector>

#include "euxis/core/credential_pool.hpp"

int main() {
    using namespace euxis::core;

    // Build a three-key pool. Real consumers load these from a secrets manager.
    std::vector<Credential> creds;
    for (auto id : {"key-a", "key-b", "key-c"}) {
        Credential c;
        c.id     = id;
        c.secret = std::string{"secret-"} + id;
        creds.push_back(std::move(c));
    }
    CredentialPool pool{std::move(creds)};

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto cred = pool.next_available(now);
    std::cout << "active: " << (cred ? *cred : "none") << '\n';

    // Simulate a 429 — cool down the active key for 60s.
    const auto reason = classify_failure(429, "rate limit; retry-after 60");
    const auto action = recovery_for(reason);
    pool.cool_down(*cred, now, /*duration_ms=*/60'000);

    std::cout << "reason: " << reason_name(reason)
              << "  action: " << (action == RecoveryAction::Retry ? "retry" : "other")
              << "  available now: " << pool.available_count(now) << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `router.hpp` | `Router` — maps intent + capability tags to an agent identifier |
| `supervisor.hpp` | `Supervisor` — state-machine for delegated subagents |
| `swarm.hpp` | `Swarm` — group operations across multiple supervisors |
| `credential_pool.hpp` | `CredentialPool` (thread-safe), `classify_failure`, `recovery_for`, `RecoveryAction`, `reason_name` |
| `delegation.hpp` | `DelegationCoordinator` — synchronous and parallel subagent fan-out |
| `types.hpp` | Shared aliases consumed by the other headers |
| `contracts.hpp` | Umbrella header — re-exports `router`, `swarm`, `types`, plus `libs/network/resilience.hpp` (`CircuitBreaker`, `RetryPolicy`) so callers can include one header |

## Examples

### Parallel subagent delegation

```cpp
#include "euxis/core/delegation.hpp"

using namespace euxis::core;

// Inject the actual subagent runner. Production wires this to a fresh
// AgentLoopHarness per subagent; tests use a mock.
DelegationCoordinator coord{
    /*subagent_fn=*/[](DelegateRequest req) {
        DelegateResult r;
        r.subagent_id = req.subagent_id;
        r.output      = "ok: " + req.instruction;
        return r;
    }};

const std::vector<DelegateRequest> work = /* N requests */;
const auto results = coord.dispatch_parallel(work);   // fan-out + join
```

### Capability-based routing

```cpp
#include "euxis/core/router.hpp"

using namespace euxis::core;

Router router;
router.register_agent("debugger",   {"debugging", "code-review"});
router.register_agent("cryptographer", {"security-audit", "compliance-check"});

const auto pick = router.resolve("Find a race in this concurrent map.");
// → "debugger"
```

## Testing

```bash
cmake --build build/cmake-build --target euxis-core-cpp_tests
./build/cmake-build/libs/core/euxis-core-cpp_tests
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
