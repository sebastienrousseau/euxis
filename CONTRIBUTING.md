# Contributing to Euxis

Welcome to the Euxis project! We appreciate your interest in contributing. This document provides guidelines and information to help you contribute effectively.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How to Contribute](#how-to-contribute)
  - [Reporting Bugs](#reporting-bugs)
  - [Suggesting Features](#suggesting-features)
  - [Pull Request Process](#pull-request-process)
- [Development Setup](#development-setup)
  - [Prerequisites](#prerequisites)
  - [Clone and Setup](#clone-and-setup)
  - [Running Tests](#running-tests)
- [Coding Standards](#coding-standards)
  - [Naming Conventions](#naming-conventions)
  - [Shell Scripts](#shell-scripts)
  - [Python](#python)
  - [Copyright Headers](#copyright-headers)
- [Commit Message Format](#commit-message-format)
- [Pull Request Checklist](#pull-request-checklist)
- [Review Process](#review-process)
- [License](#license)
- [Developer Certificate of Origin](#developer-certificate-of-origin)

## Code of Conduct

This project adheres to a Code of Conduct that all contributors are expected to follow. By participating in this project, you agree to abide by its terms. Please read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before contributing.

We are committed to providing a welcoming and inclusive environment for everyone, regardless of background or identity.

## How to Contribute

### Reporting Bugs

If you find a bug, please open an issue on GitHub with the following information:

1. **Summary**: A clear and concise description of the bug
2. **Environment**: Your operating system, shell version, Python version, and any relevant environment details
3. **Steps to Reproduce**: Detailed steps to reproduce the behavior
4. **Expected Behavior**: What you expected to happen
5. **Actual Behavior**: What actually happened
6. **Screenshots/Logs**: If applicable, add screenshots or log output to help explain the problem
7. **Possible Fix**: If you have suggestions on how to fix the bug, include them

Before submitting, please search existing issues to avoid duplicates.

### Suggesting Features

We welcome feature suggestions! To propose a new feature:

1. **Check existing issues**: Search for similar feature requests to avoid duplicates
2. **Open a new issue**: Use the feature request template if available
3. **Describe the feature**: Explain what you want and why it would be valuable
4. **Provide context**: Include use cases and examples of how the feature would be used
5. **Consider implementation**: If possible, outline how the feature might be implemented

### Pull Request Process

1. **Fork the repository** and create your branch from `main`
2. **Make your changes** following our coding standards
3. **Add or update tests** as needed
4. **Ensure all tests pass** before submitting
5. **Update documentation** if your changes affect it
6. **Submit your pull request** with a clear description of the changes

## Development Setup

### Prerequisites

Before contributing, ensure you have the following installed:

- **Bash 4.0+**: Required for shell script development
  ```bash
  bash --version
  ```

- **Python 3.8+**: Required for Python components
  ```bash
  python3 --version
  ```

- **Git**: For version control
  ```bash
  git --version
  ```

- **ShellCheck**: For shell script linting (recommended)
  ```bash
  # Ubuntu/Debian
  sudo apt-get install shellcheck

  # macOS
  brew install shellcheck
  ```

- **Python development tools**:
  ```bash
  pip install black isort ruff pytest
  ```

### Clone and Setup

1. **Fork the repository** on GitHub

2. **Clone your fork**:
   ```bash
   git clone https://github.com/YOUR_USERNAME/euxis.git
   cd euxis
   ```

3. **Add the upstream remote**:
   ```bash
   git remote add upstream https://github.com/ORIGINAL_OWNER/euxis.git
   ```

4. **Create a feature branch**:
   ```bash
   git checkout -b feat/your-feature-name
   ```

5. **Install Python dependencies** (if applicable):
   ```bash
   pip install -e ".[dev]"
   ```

### Running Tests

Run all tests before submitting your changes:

**Shell tests**:
```bash
./tests/run_shell_tests.sh
```

**Python tests**:
```bash
python -m pytest
```

**Run both**:
```bash
./tests/run_shell_tests.sh && python -m pytest
```

## Coding Standards

### Naming Conventions

Follow `docs/policies/namingConventions.md` for folder and file names. For local checks:

```bash
python scripts/check-naming-conventions.py --changed
```

If a language or toolchain requires different naming, use `config/namingExceptions.txt` and keep exceptions narrowly scoped.

### Shell Scripts

All shell scripts must be **ShellCheck compliant**. Before submitting:

1. **Run ShellCheck** on your scripts:
   ```bash
   shellcheck your_script.sh
   ```

2. **Follow best practices**:
   - Use `#!/usr/bin/env bash` as the shebang
   - Quote all variables: `"$variable"`
   - Use `[[ ]]` instead of `[ ]` for conditionals
   - Prefer `$(command)` over backticks
   - Use meaningful variable names
   - Add comments for complex logic

3. **Error handling**:
   - Use `set -euo pipefail` where appropriate
   - Check command exit codes
   - Provide meaningful error messages

### Python

Python code must comply with the following tools:

1. **Black** - Code formatting:
   ```bash
   black --check .
   black .  # to auto-format
   ```

2. **isort** - Import sorting:
   ```bash
   isort --check-only .
   isort .  # to auto-sort
   ```

3. **ruff** - Linting:
   ```bash
   ruff check .
   ruff check --fix .  # to auto-fix
   ```

**Additional Python guidelines**:
- Follow PEP 8 style guidelines
- Use type hints where practical
- Write docstrings for public functions and classes
- Keep functions focused and concise

### Copyright Headers

All source files must include a copyright header. Use the following format:

**For Shell scripts**:
```bash
#!/usr/bin/env bash
# Copyright (c) YEAR The Euxis Authors
# SPDX-License-Identifier: Apache-2.0
```

**For Python files**:
```python
# Copyright (c) YEAR The Euxis Authors
# SPDX-License-Identifier: Apache-2.0
```

## Commit Message Format

This project follows the [Conventional Commits](https://www.conventionalcommits.org/) specification. Each commit message should have the following structure:

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

### Types

- **feat**: A new feature
- **fix**: A bug fix
- **docs**: Documentation only changes
- **style**: Changes that do not affect the meaning of the code (formatting, etc.)
- **refactor**: A code change that neither fixes a bug nor adds a feature
- **perf**: A code change that improves performance
- **test**: Adding missing tests or correcting existing tests
- **build**: Changes that affect the build system or external dependencies
- **ci**: Changes to CI configuration files and scripts
- **chore**: Other changes that don't modify src or test files

### Examples

```
feat: add support for custom configuration files

fix(parser): handle edge case with empty input

docs: update installation instructions in README

refactor!: drop support for Python 3.7

BREAKING CHANGE: Python 3.8 is now the minimum required version
```

### Guidelines

- Use the imperative mood in the description ("add" not "added")
- Do not capitalize the first letter of the description
- Do not end the description with a period
- Keep the description under 72 characters
- Use the body to explain what and why, not how

## Pull Request Checklist

Before submitting your pull request, ensure the following:

- [ ] Code follows the project's coding standards
- [ ] All tests pass (`./tests/run_shell_tests.sh` and `python -m pytest`)
- [ ] ShellCheck passes on all shell scripts
- [ ] Python code passes Black, isort, and ruff checks
- [ ] Copyright headers are present in all new files
- [ ] Commit messages follow Conventional Commits format
- [ ] Documentation is updated (if applicable)
- [ ] New features include appropriate tests
- [ ] Branch is up to date with `main`
- [ ] PR description clearly explains the changes
- [ ] Related issues are referenced (e.g., "Fixes #123")

## Review Process

1. **Automated checks**: All pull requests must pass CI checks before review
2. **Code review**: At least one maintainer will review your changes
3. **Feedback**: Address any requested changes or feedback
4. **Approval**: Once approved, a maintainer will merge your PR
5. **Merge**: PRs are typically squash-merged to maintain a clean history

### What reviewers look for

- Code quality and adherence to standards
- Test coverage and quality
- Documentation completeness
- Potential security issues
- Performance considerations
- Compatibility with existing code

### Response time

We aim to provide initial feedback within a few days. Complex changes may require more time. Please be patient, and feel free to ping the thread if you haven't heard back in a week.

## License

This project is licensed under the **Apache License 2.0**. By contributing to this project, you agree that your contributions will be licensed under the same license.

See [LICENSE](LICENSE) for the full license text.

### What this means for contributors

- You retain copyright to your contributions
- You grant the project a perpetual, worldwide, non-exclusive, royalty-free license to use your contributions
- Your contributions may be used, modified, and distributed by anyone under the Apache 2.0 license terms

## Developer Certificate of Origin

By contributing to this project, you certify that:

1. The contribution was created in whole or in part by you and you have the right to submit it under the open source license indicated in the file; or

2. The contribution is based upon previous work that, to the best of your knowledge, is covered under an appropriate open source license and you have the right under that license to submit that work with modifications, whether created in whole or in part by you, under the same open source license (unless you are permitted to submit under a different license), as indicated in the file; or

3. The contribution was provided directly to you by some other person who certified (1), (2) or (3) and you have not modified it.

4. You understand and agree that this project and the contribution are public and that a record of the contribution (including all personal information you submit with it, including your sign-off) is maintained indefinitely and may be redistributed consistent with this project or the open source license(s) involved.

### Sign-off

To indicate your agreement with the DCO, please add a "Signed-off-by" line to your commit messages:

```
Signed-off-by: Your Name <your.email@example.com>
```

You can do this automatically by using the `-s` flag when committing:

```bash
git commit -s -m "feat: your commit message"
```

---

Thank you for contributing to Euxis! Your efforts help make this project better for everyone.
