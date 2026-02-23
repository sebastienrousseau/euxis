# Security Integration Guide

Integrate automated dependency scanning into Euxis submodule CI/CD pipelines effortlessly utilizing this guide.

## Overview

The Euxis project enforces multi-layered security scanning:

1. **Centralized Security Scan:** Defined in `.github/workflows/security-scan.yml`.
2. **Per-Module CI Integration:** Required across individual workflow definitions.
3. **Security Policy Enforcement:** Fail builds immediately on critical vulnerability identification.
4. **Regular Scheduled Scanning:** Execute automated full-repository scans weekly.

## Architecture

```
┌─────────────────────┐    ┌──────────────────────┐    ┌─────────────────────┐
│   Pull Request      │    │   Centralized        │    │   Individual        │
│   Triggered         │───▶│   Security Scan      │    │   Module CI         │
│   Security Check    │    │   (All Modules)      │    │   (Per Module)      │
└─────────────────────┘    └──────────────────────┘    └─────────────────────┘
           │                           │                           │
           │                           │                           │
           ▼                           ▼                           ▼
┌─────────────────────┐    ┌──────────────────────┐    ┌─────────────────────┐
│   Security Gate     │    │   Weekly Scheduled   │    │   Fast Security     │
│   (Block on High)   │    │   Full Scan          │    │   Gate Integration  │
└─────────────────────┘    └──────────────────────┘    └─────────────────────┘
```

## Integration Steps

Configure CI/CD pipelines natively. 

### 1. Update CI Workflow

Add the security scanning job explicitly to `.github/workflows/ci.yml` within each submodule.

```yaml
jobs:
  security:
    name: Security Scan
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.12'

      - name: Install security tools
        run: |
          python -m pip install --upgrade pip
          pip install safety==3.0.1 bandit[toml]==1.7.5 pip-audit==2.6.3

      - name: Install project dependencies
        run: pip install -e .[dev]

      - name: Run dependency vulnerability scan
        run: |
          pip freeze > requirements-temp.txt
          safety check -r requirements-temp.txt --json --output safety-report.json || true
          pip-audit --desc --format=json --output=pip-audit.json . || true

      - name: Run code security analysis
        run: |
          if [[ -d src ]]; then
            bandit -r src/ -f json -o bandit-report.json || true
          fi

  test:
    needs: security
    # Remaining configuration here
```

### 2. Security Gate Configuration

Evaluate scan artifacts systematically inside a security gate job.

**Gotchas:** Fails fatally if critical or high vulnerabilities are detected.

```yaml
  security-gate:
    name: Security Gate
    runs-on: ubuntu-latest
    needs: [security, test]
    if: always()
    steps:
      - name: Download security reports
        uses: actions/download-artifact@v4
        with:
          name: security-reports-{module-name}

      - name: Evaluate security posture
        run: |
          if [[ -f safety-report.json ]]; then
            CRITICAL_VULNS=$(jq '.vulnerabilities[]? | select(.vulnerability.severity == "high" or .vulnerability.severity == "critical")' safety-report.json | jq -s length)
            if [[ $CRITICAL_VULNS -gt 0 ]]; then
              echo "::error::Security gate failed - $CRITICAL_VULNS critical/high vulnerabilities found"
              exit 1
            fi
          fi
```

## Security Tools

### Python Dependencies

| Tool | Purpose | Configuration |
|------|---------|---------------|
| `safety` | Known vulnerability database | Default settings |
| `pip-audit` | Comprehensive package audit | JSON output |
| `bandit` | Static code security analysis | `.bandit.yml` |
| `semgrep` | Advanced pattern matching | Auto ruleset |

### Node.js Dependencies

| Tool | Purpose | Configuration |
|------|---------|---------------|
| `npm audit` | Node vulnerability database | High severity threshold |
| `semgrep` | JavaScript/TypeScript analysis | Auto ruleset |

## Security Policy

Enforce policies efficiently to block compromised vectors. 

### Build Failures

- **Critical/High Array:** Terminate build immediately.
- **Medium Array:** Issue warning only; build continues seamlessly.
- **Low Array:** Log for tracking exclusively.

### Scan Frequency

- **Pull Requests**: Every PR triggers a synchronous security scan.
- **Push to Main**: Scan continuously upon merge execution.
- **Scheduled**: Execute a weekly comprehensive scan (Sundays 2 AM UTC).

### Exception Handling

Configure specific exclusions purposefully:
- `.bandit.yml` for Bandit.
- `.semgrepignore` for Semgrep.

## Implementation Examples

### euxis-core Integration

Review `.github/workflows/ci.yml` within `euxis-core` to observe a complete production integration. This includes artifact upload capabilities and strict gating logic.

### Quick Integration Script

Execute `add-security-scan.sh` to inject security checks directly into arbitrary submodules.

```bash
#!/bin/bash
MODULE_PATH="$1"
if [[ -z "$MODULE_PATH" ]]; then
    echo "Usage: $0 <module-path>"
    exit 1
fi

CI_FILE="$MODULE_PATH/.github/workflows/ci.yml"
if [[ ! -f "$CI_FILE" ]]; then
    echo "Error: CI workflow not found at $CI_FILE"
    exit 1
fi

cp "$CI_FILE" "$CI_FILE.backup"
echo "Security scanning added to $MODULE_PATH"
```

## Monitoring and Reporting

Generate discrete JSON reports for continuous logging:
- `safety-report.json`
- `pip-audit.json`
- `bandit-report.json`
- `semgrep-report.json`

## Troubleshooting Options

1. **False Positives**: Assign paths to the corresponding ignore file.
2. **Tool Version Conflicts**: Pin toolchain versions deterministically within the workflow block.
3. **Report Parsing**: Ensure JSON payload exports are formatted correctly before gating.

Enable pipeline debug output if the trace fails unexpectedly by setting `RUNNER_DEBUG: 1` as an environment variable.