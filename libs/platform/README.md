<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::platform</h1>

<p align="center">
  Platform abstraction for euxis: <code>IExecutionBackend</code> with
  Local + Docker implementations, OS-specific <code>Platform</code> probes
  (Linux, macOS, WSL), and a sandbox profile enum consumed by the bridge
  pipeline.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — run a command via the local backend
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/platform)
target_link_libraries(my_app PRIVATE euxis-platform-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/platform/execution_backend.hpp"

int main() {
    using namespace euxis::platform;

    LocalBackend backend;

    ExecutionRequest req;
    req.argv     = {"git", "rev-parse", "HEAD"};
    req.timeout  = std::chrono::seconds{5};

    const auto result = backend.run(req);
    if (result.exit_code != 0) {
        std::cerr << "stderr: " << result.stderr_output << '\n';
        return 1;
    }
    std::cout << "HEAD: " << result.stdout_output << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `execution_backend.hpp` | `IExecutionBackend` (abstract), `LocalBackend` (fork/exec/poll, drains stdout+stderr concurrently with a deadline), `DockerBackend` (shells out to the `docker` CLI; no libdocker dep), `ExecutionRequest`, `ExecutionResult` |
| `platform.hpp` | `Platform` base + `LinuxPlatform`, `MacOSPlatform`, `WSLPlatform`; `PlatformType` enum; `detect_platform()` |
| `sandbox.hpp` | `SandboxProfile` enum (Default, Permissive, Strict), `SandboxResult` |

Deliberately out of scope: eBPF, KVM, microVMs. The two execution backends shipped are the two we tested. SSH / Singularity / Modal / Daytona / Vercel are not implemented.

## Examples

### Choose a backend per skill policy

```cpp
#include <memory>

#include "euxis/platform/execution_backend.hpp"

std::unique_ptr<IExecutionBackend> backend;
if (policy.requires_isolation) {
    backend = std::make_unique<DockerBackend>(DockerBackend::Config{
        .image = "euxis-runner:latest",
    });
} else {
    backend = std::make_unique<LocalBackend>();
}
```

### Probe the host platform

```cpp
#include "euxis/platform/platform.hpp"

auto platform = detect_platform();
if (platform->type() == PlatformType::WSL) {
    // route clipboard / URL operations to Windows equivalents
}
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
