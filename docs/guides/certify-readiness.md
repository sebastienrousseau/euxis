# Certify Readiness Guide

`euxis certify-readiness` assesses whether a repository meets certification
readiness criteria across 18 quality domains with optional framework overlays.

## Quick Start

```bash
# Basic readiness check
euxis certify-readiness .

# With SOC2 framework overlay
euxis certify-readiness --framework soc2 .

# CI mode with JSON artifact
euxis certify-readiness --ci --json .

# Strict mode (all gates blocking)
euxis certify-readiness --strict .
```

## Gates

Five hard gates determine overall readiness status:

| Gate | Default | Description |
|------|---------|-------------|
| `commit_signing` | non-blocking | GPG/SSH commit signature verification |
| `unit_test_health` | blocking | Test suite passes with adequate coverage |
| `build_integrity` | blocking | Clean build with no errors |
| `documentation_accuracy` | non-blocking | Docs match actual code state |
| `security_critical` | blocking | No critical security findings |

In `--strict` mode, all gates become blocking.

## Domains

The assessment evaluates 18 quality domains:

- **Code Quality**: complexity, duplication, style consistency
- **Testing**: coverage, test health, test diversity
- **Security**: vulnerability scan, secret detection, dependency audit
- **Documentation**: README accuracy, API docs, changelog freshness
- **Architecture**: modularity, dependency management
- **Operations**: CI/CD, monitoring, deployment readiness

Each domain receives a 0-100 score based on available evidence.

## Framework Overlays

### General (default)
4 critical controls focused on basic software quality.

### SOC2
9 critical controls aligned with Trust Services Criteria:
- Access control, change management, risk assessment
- Incident response, monitoring, availability

### ISO 27001
9 critical controls aligned with Annex A:
- Information security policy, asset management
- Access control, cryptography, operations security

## Interpreting Results

| Status | Exit Code | Meaning |
|--------|-----------|---------|
| READY | 0 | All blocking gates pass |
| READY WITH GAPS | 0 | Blocking gates pass, non-blocking gaps exist |
| BLOCKED | 1 | One or more blocking gates fail |
| INCONCLUSIVE | 1 | Insufficient evidence to assess |

## CI Integration

```yaml
# GitHub Actions example
- name: Certification Check
  run: euxis certify-readiness --ci --json --framework soc2 .
  env:
    EUXIS_HOME: ${{ github.workspace }}/.euxis

# GitLab CI example
certification:
  script:
    - euxis certify-readiness --ci --json --output cert.json .
  artifacts:
    paths: [cert.json]
```

## Subcommands

### `controls`
List all controls for the selected framework:
```bash
euxis certify-readiness controls --framework soc2
```

### `report <artifact>`
Generate a human-readable report from a certification artifact:
```bash
euxis certify-readiness report data/runtime/sessions/latest_certification.json
```

## Flags Reference

| Flag | Description |
|------|-------------|
| `--framework` | Framework overlay: general, soc2, iso27001 |
| `--strict` | Make all gates blocking |
| `--ci` | CI mode (JSON stdout, no interactive output) |
| `--json` | Emit structured JSON artifact |
| `--no-build` | Skip build integrity gate |
| `--no-tests` | Skip unit test health gate |
| `--no-security` | Skip security gate |
| `--commit-window N` | Check last N commits (default: all) |
| `--since-ref REF` | Check commits since git ref |
| `--output PATH` | Write artifact to specific path |

## Disclaimer

This is an internal readiness assessment tool. Results do not constitute
official certification. Use `assessment_type: internal_readiness` in the
artifact schema.
