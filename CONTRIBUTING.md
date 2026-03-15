# Contributing to Euxis

Thank you for contributing. This guide covers everything needed to submit high-quality changes.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How to Contribute](#how-to-contribute)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Repository Standards](#repository-standards)
- [Commit Signing](#commit-signing)
- [Commit Message Format](#commit-message-format)
- [Pull Request Checklist](#pull-request-checklist)
- [Review Process](#review-process)
- [License](#license)
- [Developer Certificate of Origin](#developer-certificate-of-origin)

## Code of Conduct

This project adheres to a Code of Conduct. By participating, you agree to its terms. Read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before contributing.

## How to Contribute

### Reporting Bugs

Open an issue on GitHub with:

1. **Summary**: Clear description of the bug
2. **Environment**: Operating system, compiler version, CMake version
3. **Steps to Reproduce**: Detailed steps
4. **Expected vs Actual Behavior**: What happened and what should have happened
5. **Logs**: Relevant output or screenshots
6. **Possible Fix**: Suggestions, if any

Search existing issues first to avoid duplicates.

### Suggesting Features

1. Search for similar feature requests
2. Open a new issue using the feature request template
3. Describe the feature, its value, and potential implementation

### Pull Request Process

1. Fork the repository and create a branch from `main`
2. Make changes following the coding standards below
3. Add or update tests as needed
4. Ensure all tests pass before submitting
5. Update documentation if affected
6. Submit the pull request with a clear description

## Development Setup

### Prerequisites

| Tool | Minimum | Check | Install |
|------|---------|-------|---------|
| CMake | 3.28+ | `cmake --version` | [cmake.org](https://cmake.org/download/) |
| C++ compiler | GCC 14+ or Clang 18+ | `g++ --version` | See below |
| Git | 2.x+ | `git --version` | [git-scm.com](https://git-scm.com/) |

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

### Clone and Build

1. **Fork the repository** on GitHub

2. **Clone the fork:**
   ```bash
   git clone https://github.com/YOUR_USERNAME/euxis.git
   cd euxis
   ```

3. **Add the upstream remote:**
   ```bash
   git remote add upstream https://github.com/sebastienrousseau/euxis.git
   ```

4. **Create a feature branch:**
   ```bash
   git checkout -b feat/your-feature-name
   ```

5. **Build:**
   ```bash
   make cpp-configure
   make cpp-build
   ```

### Running Tests

Run all tests before submitting changes:

```bash
make cpp-test
```

Or run the test binary directly for filtered output:

```bash
build/cmake-build/cmd/cli/euxis-cli-cpp_tests
build/cmake-build/cmd/cli/euxis-cli-cpp_tests --gtest_filter="YourTest.*"
```

### Formatting

```bash
make cpp-format    # clang-format on all .cpp/.hpp files
```

## Coding Standards

### C++

Euxis is a C++23 project. Follow these conventions:

- **Standard**: C++23 with `-Werror` enabled
- **Naming**: `snake_case` for functions and variables, `PascalCase` for types and classes
- **Headers**: `.hpp` for C++ headers, `.h` for C-compatible headers
- **Namespaces**: `euxis::[module]` (e.g., `euxis::cli`, `euxis::core`)
- **Includes**: Use `#pragma once` for include guards
- **Comments**: Explain *why*, not *what*. Skip comments for self-evident code.

### File and Directory Naming

- **Directories and files**: `kebab-case` (e.g., `provider-router.cpp`)
- **C++ source pairs**: `feature.hpp` + `feature.cpp`

### Copyright Headers

All source files must include a copyright header:

**C++ files:**
```cpp
// Copyright (c) YEAR The Euxis Authors
// SPDX-License-Identifier: AGPL-3.0-only
```

**Shell scripts:**
```bash
#!/usr/bin/env bash
# Copyright (c) YEAR The Euxis Authors
# SPDX-License-Identifier: AGPL-3.0-only
```

## Repository Standards

### Directory Roles

| Directory | Role | Rules |
|-----------|------|-------|
| `cmd/` | Application entry points | Thin wrappers around `pkg/` |
| `pkg/` | SDK libraries | No `main()`. Reusable across apps. |
| `internal/` | Platform abstraction | The only place for `#ifdef __APPLE__` / `#ifdef __linux__` |
| `data/` | Read-only data | Config, schemas, agent prompts, docs |

### The Gold Rule

No circular dependencies. If `Lib A` needs `Lib B` and `Lib B` needs `Lib A`,
extract the shared logic into `Lib C` or move it to `core`.

## Commit Signing

All commits must be cryptographically signed. This is enforced by CI and the `certify-readiness` gate.

**Setup (one-time):**

```bash
# Generate a GPG key
gpg --full-generate-key

# Find the key ID
gpg --list-secret-keys --keyid-format=long

# Configure Git to sign all commits
git config --global user.signingkey <KEY_ID>
git config --global commit.gpgsign true
```

<details>
<summary><strong>macOS: pinentry setup</strong></summary>

```bash
brew install pinentry-mac
echo "pinentry-program $(which pinentry-mac)" >> ~/.gnupg/gpg-agent.conf
gpgconf --kill gpg-agent
```

</details>

**Verify the setup:**

```bash
echo "test" | gpg --clearsign       # Should produce signed output
git log --show-signature -1          # Should show "Good signature"
```

Unsigned commits will not pass CI or code review.

## Commit Message Format

Follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

### Types

| Type | Purpose |
|------|---------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation changes |
| `refactor` | Code change (no bug fix, no new feature) |
| `perf` | Performance improvement |
| `test` | Test additions or corrections |
| `build` | Build system or dependency changes |
| `ci` | CI configuration changes |
| `chore` | Other changes (no src or test modifications) |

### Guidelines

- Imperative mood: "add" not "added"
- Lowercase first letter
- No period at the end
- Under 72 characters
- Body explains *what* and *why*, not *how*

### Examples

```
feat(router): add provider strategy routing

fix(certification): handle missing CHANGELOG gracefully

docs: update quick-start for C++23 build workflow
```

## Pull Request Checklist

Before submitting:

- [ ] All tests pass (`make cpp-test`)
- [ ] Code compiles without warnings (`-Werror` is enabled)
- [ ] All commits are signed (`git log --show-signature`)
- [ ] Commit messages follow Conventional Commits format
- [ ] Copyright headers are present in new files
- [ ] Documentation is updated if affected
- [ ] New features include tests
- [ ] Branch is up to date with `main`
- [ ] PR description clearly explains the changes
- [ ] Related issues are referenced (e.g., "Fixes #123")

## Review Process

1. **Automated checks**: CI must pass before review
2. **Code review**: At least one maintainer reviews changes
3. **Feedback**: Address requested changes
4. **Approval**: A maintainer merges the PR (typically squash-merged)

### What reviewers look for

- Code quality and adherence to C++23 standards
- Test coverage and quality
- Documentation completeness
- Security considerations
- Performance impact
- Compatibility with existing code

Initial feedback is typically provided within a few days.

## License

This project is licensed under the **AGPL-3.0 License**. By contributing, you agree that contributions will be licensed under the same terms.

See [LICENSE](LICENSE) for the full text.

### What this means

- Copyright is retained by contributors
- A perpetual, worldwide, royalty-free license is granted to the project
- Contributions may be used, modified, and distributed under AGPL-3.0 terms

## Developer Certificate of Origin

By contributing, you certify that:

1. The contribution was created in whole or in part by you and you have the right to submit it under the open source license indicated in the file; or

2. The contribution is based upon previous work covered under an appropriate open source license and you have the right to submit it with modifications under the same license; or

3. The contribution was provided to you by someone who certified (1), (2), or (3) and you have not modified it.

4. You understand that this project and the contribution are public and that a record of the contribution is maintained indefinitely.

### Sign-off

Add a "Signed-off-by" line to commit messages:

```
Signed-off-by: Your Name <your.email@example.com>
```

Use the `-s` flag automatically:

```bash
git commit -s -S -m "feat: your commit message"
```

The `-s` flag adds the DCO sign-off. The `-S` flag signs the commit with GPG.

---

Thank you for contributing to Euxis.
