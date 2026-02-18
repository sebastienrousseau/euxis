# Security Integration Guide

This document explains how to integrate automated dependency scanning into Euxis submodule CI/CD pipelines.

## Overview

The Euxis project implements multi-layered security scanning:

1. **Centralized Security Scan** (`.github/workflows/security-scan.yml`)
2. **Per-Module CI Integration** (individual workflow updates)
3. **Security Policy Enforcement** (fail builds on critical vulnerabilities)
4. **Regular Scheduled Scanning** (weekly automated scans)

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

### 1. Update CI Workflow

For each submodule, add security scanning to the existing `.github/workflows/ci.yml`:

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

  # Update existing test job to depend on security
  test:
    needs: security
    # ... rest of existing test configuration
```

### 2. Security Gate Configuration

Add a security gate job that evaluates scan results:

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
          # Check for critical/high vulnerabilities
          if [[ -f safety-report.json ]]; then
            CRITICAL_VULNS=$(cat safety-report.json | jq '.vulnerabilities[]? | select(.vulnerability.severity == "high" or .vulnerability.severity == "critical")' | jq -s length)
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

Security scanning follows these rules:

### Build Failures

- **Critical vulnerabilities**: Build fails immediately
- **High vulnerabilities**: Build fails immediately
- **Medium vulnerabilities**: Warning only, build continues
- **Low vulnerabilities**: Tracking only

### Scan Frequency

- **Pull Requests**: Every PR triggers security scan
- **Push to Main**: Security scan on merge
- **Scheduled**: Weekly full scan (Sundays 2 AM UTC)
- **Manual**: `workflow_dispatch` trigger available

### Exception Handling

Acceptable security patterns in test code:
- Hardcoded test credentials
- Mock security functions
- Development-only debug output

Configure exceptions in:
- `.bandit.yml` for Bandit
- `.semgrepignore` for Semgrep

## Implementation Examples

### euxis-core Integration (Complete)

See `.github/workflows/ci.yml` in `euxis-core` for a complete implementation including:
- Dependency scanning with Safety and pip-audit
- Code security analysis with Bandit
- Security gate with failure conditions
- Artifact upload for report retention

### Quick Integration Script

Use this script to quickly add security scanning to a submodule:

```bash
#!/bin/bash
# add-security-scan.sh - Add security scanning to CI workflow

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

# Backup original
cp "$CI_FILE" "$CI_FILE.backup"

# Add security job before existing test job
# Implementation would insert the security scanning configuration
echo "Security scanning added to $MODULE_PATH"
echo "Please review and test the updated workflow"
```

## Monitoring and Reporting

### Security Reports

Each scan generates:
- `safety-report.json` - Dependency vulnerabilities
- `pip-audit.json` - Comprehensive package audit
- `bandit-report.json` - Code security issues
- `semgrep-report.json` - Pattern-based findings

### Security Summary

The centralized security scan generates:
- Per-module vulnerability counts
- Severity breakdown
- Trend analysis over time
- Action items for remediation

### Integration with Security Policy

All scanning follows the security policy defined in:
- `.github/SECURITY.md` - Human-readable policy
- `security-policy.json` - Machine-readable configuration
- `.bandit.yml` - Bandit-specific rules
- `.semgrepignore` - Semgrep exclusions

## Troubleshooting

### Common Issues

1. **False Positives**: Add to appropriate ignore file
2. **Tool Version Conflicts**: Pin specific versions in workflow
3. **Performance Issues**: Use caching for pip dependencies
4. **Report Parsing**: Ensure JSON outputs are valid

### Debug Mode

Enable debug output by setting:
```yaml
env:
  RUNNER_DEBUG: 1
```

## Best Practices

1. **Pin Tool Versions**: Avoid surprises from tool updates
2. **Cache Dependencies**: Speed up CI runs
3. **Parallel Execution**: Run security scans in parallel with tests when possible
4. **Meaningful Names**: Use descriptive job and step names
5. **Artifact Retention**: Keep security reports for audit trails

## Future Enhancements

Planned improvements:
- [ ] SARIF output format for GitHub Code Scanning
- [ ] Integration with dependency update automation (Dependabot)
- [ ] Custom security rules for Euxis-specific patterns
- [ ] Trend analysis and security metrics dashboard
- [ ] Integration with external security platforms

---

**Last Updated**: 2026-02-18
**Maintainer**: Sebastien Rousseau <sebastien.rousseau@gmail.com>