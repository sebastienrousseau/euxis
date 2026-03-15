# Quick Start

**Clone, build, and verify in 5 minutes.**

---

## What This Guide Covers

By the end:

- Euxis compiled and installed
- First verification run complete
- Understanding of core commands and modes

---

## Prerequisites

| Tool | Minimum | Check |
|------|---------|-------|
| CMake | 3.28+ | `cmake --version` |
| C++ compiler | GCC 14+ or Clang 18+ | `g++ --version` or `clang++ --version` |
| Git | 2.x+ | `git --version` |
| AI provider | At least one | See [Providers](#ai-providers) |

### AI Providers

Euxis routes tasks to the optimal provider. At least one must be available:

| Provider | Best For | Setup |
|:---------|:---------|:------|
| [Ollama](https://ollama.com/) | Local inference, free | `curl -fsSL https://ollama.ai/install.sh \| sh` |
| [Claude](https://docs.anthropic.com/en/docs/claude-cli) | Coding, architecture, audit | Set `ANTHROPIC_API_KEY` |
| [Gemini](https://github.com/google-gemini/gemini-cli) | Research, security, large context | Set `GEMINI_API_KEY` |
| [OpenAI](https://platform.openai.com/) | Research, synthesis | Set `OPENAI_API_KEY` |

<details>
<summary><strong>Additional providers</strong></summary>

| Provider | Purpose |
|----------|---------|
| [Aider](https://aider.chat/) | Surgical code edits |
| [Kiro](https://kiro.dev) | Terminal automation |
| [ShellGPT](https://github.com/TheR1D/shell_gpt) | Shell command generation |

</details>

<details>
<summary><strong>Platform-specific compiler install</strong></summary>

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

---

## Step 1: Clone and Build

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
cd ~/.euxis
make cpp-configure
make cpp-build
make cpp-test
```

Build output: `build/cmake-build/cmd/cli/euxis-cli`.

---

## Step 2: Add to PATH

Pick one method:

```bash
# Symlink (recommended)
sudo ln -sf ~/.euxis/build/cmake-build/cmd/cli/euxis-cli /usr/local/bin/euxis
```

<details>
<summary><strong>Alternative: add to shell profile</strong></summary>

**Bash / Zsh:**
```bash
echo 'export PATH="$HOME/.euxis/build/cmake-build/cmd/cli:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

**Fish:**
```bash
fish_add_path ~/.euxis/build/cmake-build/cmd/cli
```

</details>

---

## Step 3: Verify

```bash
euxis doctor
```

Doctor checks the platform, required tools, data directories, agent registry, and available AI providers.

---

## Step 4: First Run

Run a quick triage scan on any repository:

```bash
euxis triage .
```

This deploys 2 agents (librarian + reviewer) in flash mode with a 75-second budget. The output is a structured verdict with confidence score and findings.

---

## Step 5: Go Deeper

```bash
# Standard verification (~3 minutes, 5 agents)
euxis check .

# Forensic-depth review (all 11 agents)
euxis review . --forensic

# Compare triage vs standard side-by-side
euxis compare .

# Certification readiness (18 domains, framework overlays)
euxis certify-readiness . --framework general

# View recent metrics and drift
euxis stats --last 5
```

### Modes

| Mode | Agents | Budget | Use Case |
|------|--------|--------|----------|
| **Flash** | 2 | 45-75s | Quick screening, CI gates |
| **Standard** | 5 | 3-5 min | Pre-merge verification |
| **Forensic** | 11 | 10-15 min | Release audits, deep analysis |

---

## Core Commands

| Command | What It Does |
|---------|-------------|
| `euxis check [target]` | Verify a repository (standard mode) |
| `euxis triage [target]` | Fast bounded triage (~45 seconds) |
| `euxis review [target]` | Deep verification (standard or forensic) |
| `euxis certify-readiness [target]` | Certification readiness assessment |
| `euxis compare <target>` | Compare triage against deep verification |
| `euxis stats` | Validation metrics and drift history |
| `euxis policy <sub>` | Policy inspection and enforcement |
| `euxis doctor` | Environment diagnostics |

### Aliases

| Alias | Resolves To |
|-------|-------------|
| `quick` | `triage` |
| `deep` | `review` |
| `diag` | `doctor` |
| `metrics` | `stats` |

Run `euxis --help` for the full command list across 8 groups.

---

## Advanced

```bash
# Full verification playbook
euxis playbook verify-everything .

# Multi-agent pipeline
euxis combo run envision "Design a notification system"

# Squad deployment
euxis squad deploy quality "Audit the auth module"

# Desktop GUI
euxis-etx
```

---

## What's Next

| Guide | Content |
|-------|---------|
| [User Guide](../guides/user-guide.md) | Complete CLI reference and modes |
| [CLI Reference](../reference/cli-reference.md) | Every command, flag, and example |
| [Fleet Guide](../guides/fleet-guide.md) | Agent roster and provider routing |
| [Choosing Coordination](core-concepts/choosing-coordination.md) | Agents vs combos vs squads vs playbooks |

---

## Troubleshooting

<details>
<summary><strong>Command not found: euxis</strong></summary>

Verify the binary exists and PATH is configured:

```bash
ls ~/.euxis/build/cmake-build/cmd/cli/euxis-cli
echo $PATH | grep euxis
```

If the binary exists but PATH is wrong, re-run the symlink or profile step above. Then restart the terminal.

</details>

<details>
<summary><strong>Build fails</strong></summary>

Check compiler version:
```bash
g++ --version   # Need 14+
cmake --version  # Need 3.28+
```

On Ubuntu, install a newer GCC:
```bash
sudo apt install -y g++-14
export CXX=g++-14
make cpp-clean && make cpp-configure && make cpp-build
```

</details>

<details>
<summary><strong>No AI provider available</strong></summary>

Install Ollama for free local inference:
```bash
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull qwen2.5-coder:7b
ollama list
```

Or set an API key for a cloud provider:
```bash
export ANTHROPIC_API_KEY="sk-..."
export OPENAI_API_KEY="sk-..."
export GEMINI_API_KEY="..."
```

</details>

---

*Euxis v0.0.4*
