# Euxis

High-performance agent OS. Built in C++23.

[![Version][version-badge]][version-url] [![License][license-badge]][license-url] [![Docs](https://img.shields.io/badge/docs-gh--pages-blue)](https://sebastienrousseau.github.io/euxis/)

Deploy, orchestrate, and observe AI agents at native speed.
Euxis replaces interpreted agent loops with compiled execution —
sub-10ms dispatch, zero-copy memory, cryptographic provenance.

## Get Started

### Prerequisites

| Tool | Minimum | Check |
|------|---------|-------|
| CMake | 3.28+ | `cmake --version` |
| C++ compiler | GCC 14+ or Clang 18+ | `g++ --version` or `clang++ --version` |
| Git | 2.x+ | `git --version` |

**Platform support:** macOS (Apple Clang or Homebrew GCC), Linux (GCC/Clang), WSL (Ubuntu GCC).

<details>
<summary><strong>Platform-specific setup</strong></summary>

**macOS:**
```bash
brew install cmake gcc
```

**Ubuntu / Debian / WSL:**
```bash
sudo apt update && sudo apt install -y cmake g++-14 git
```

**Arch Linux:**
```bash
sudo pacman -S cmake gcc git
```

</details>

### Build

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
cd ~/.euxis
make cpp-configure
make cpp-build
make cpp-test       # 2000+ tests across 19 suites
```

Binaries are in `cmake-build/apps/cli/`.

### Add to PATH

Pick one method:

```bash
# Symlink (recommended)
sudo ln -sf ~/.euxis/cmake-build/apps/cli/euxis-cli /usr/local/bin/euxis

# Or add to shell profile
# Bash / Zsh:
echo 'export PATH="$HOME/.euxis/cmake-build/apps/cli:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Fish:
fish_add_path ~/.euxis/cmake-build/apps/cli
```

### Verify

```bash
euxis doctor
```

## Core Commands

| Command | What It Does |
|---------|-------------|
| `euxis check [target]` | Verify a repository (standard mode) |
| `euxis triage [target]` | Fast bounded triage (~45 seconds) |
| `euxis review [target]` | Deep verification (standard or forensic) |
| `euxis certify-readiness [target]` | Certification readiness assessment (18 domains) |
| `euxis compare <target>` | Compare triage against deep verification |
| `euxis stats` | Validation metrics and drift history |
| `euxis policy <sub>` | Policy inspection and enforcement |

```bash
euxis triage .                    # Fast scan
euxis check .                     # Standard verification (~3 min)
euxis review . --forensic         # Forensic depth
euxis certify-readiness . --framework soc2   # SOC2 readiness
euxis compare .                   # Side-by-side comparison
euxis stats --last 5              # Recent metrics
euxis doctor                      # Environment diagnostics
```

### Aliases

| Alias | Resolves To |
|-------|-------------|
| `quick` | `triage` |
| `deep` | `review` |
| `diag` | `doctor` |
| `metrics` | `stats` |
| `pb` | `playbook` |
| `verify-all` | `check` |

### All Command Groups

Run `euxis --help` for the full list across 8 groups:
**Core** · Lifecycle · System · Fleet · Knowledge · Infrastructure · Development · Specialized.

## Advanced

```bash
euxis playbook verify-everything .       # Full verification playbook
euxis combo run envision "Design X"      # Multi-agent pipeline
euxis certify-readiness . --strict       # Strict certification
euxis-etx                                # Desktop GUI (Qt6)
```

## Architecture

```
apps/             Application layer
  cli/            Command-line interface (58 commands, 8 groups)
  etx/            Qt6 desktop GUI (17 screens)
  gateway/        HTTP/WebSocket server
  publisher/      Document rendering engine

libs/             SDK layer
  core/           Execution engine, FinOps routing, swarm orchestration
  runtime/        Agent lifecycle and scheduling
  security/       Threat detection and policy enforcement
  network/        MCP client, A2A transports
  crypto/         AES-256-GCM, Ed25519, Argon2id
  metrics/        Telemetry and validation framework
  inference/      Local inference via llama.cpp
  platform/       Platform abstraction (macOS, Linux, WSL)

data/             Configuration, agent prompts, playbooks, docs
```

### Provider Strategy

Euxis routes tasks to the optimal AI provider based on semantic classification:

| Task Class | Primary Provider | Fallback |
|-----------|-----------------|----------|
| Research / synthesis | OpenAI | Gemini, Claude |
| Coding / architecture / audit | Claude | Gemini, Ollama |
| Deep research / security | Gemini | OpenAI, Claude |
| Private / local | Ollama | — |
| Surgical edits | Aider | Claude, Ollama |
| Terminal automation | Kiro | ShellGPT, Claude |

Configuration: [`data/config/provider_strategy.json`](data/config/provider_strategy.json).
Override with environment variables: `EUXIS_DEFAULT_RESEARCH_PROVIDER`, `EUXIS_DEFAULT_CODING_PROVIDER`, `EUXIS_DEFAULT_SECURITY_PROVIDER`.

## Building from Source

```bash
make cpp-configure    # CMake configure (Release, LTO)
make cpp-build        # Build all targets
make cpp-test         # Run test suite (ctest)
make cpp-clean        # Remove build artifacts
make cpp-format       # Format with clang-format
make cpp-coverage     # Build with coverage (requires lcov)
```

Override parallelism: `make cpp-build CPP_BUILD_JOBS=8`.

## Documentation

| Guide | Content |
|-------|---------|
| [Quick Start](data/docs/essentials/quick-start.md) | Clone to verified in 5 minutes |
| [User Guide](data/docs/guides/user-guide.md) | Complete CLI reference and modes |
| [CLI Reference](data/docs/reference/cli-reference.md) | Every command, flag, and example |
| [Fleet Guide](data/docs/guides/fleet-guide.md) | Agent roster and routing |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). All commits must be [signed](CONTRIBUTING.md#commit-signing).

## License

AGPL-3.0 — see [LICENSE](LICENSE).

---

Euxis v0.0.10 · [euxis.co](https://euxis.co)

[license-badge]: https://img.shields.io/badge/license-AGPL--3.0-blue.svg
[license-url]: LICENSE
[version-badge]: https://img.shields.io/badge/version-v0.0.10-green.svg
[version-url]: https://github.com/sebastienrousseau/euxis/releases/tag/v0.0.10
