# Evidence Verification Framework

## Overview

The Evidence Verification Framework ensures that strategic analysis outputs are backed by credible source documentation. This framework validates that quantitative claims, financial projections, and strategic assertions are properly evidenced according to Euxis standards.

## Framework Components

### 1. Evidence Validator (`evidence_validator.py`)

A Python-based validation pipeline that automatically scans strategic documents for claims requiring evidence and verifies that appropriate sources are cited.

### 2. Evidence Requirements

The framework defines specific evidence requirements for different types of claims:

| Claim Type | Required Sources | Severity |
|------------|------------------|----------|
| **Percentages/Statistics** | Measurement, benchmark, survey, study | HIGH |
| **Financial Figures** | Financial report, budget, invoice, estimate | CRITICAL |
| **Market Claims** | Market research, competitor analysis, industry report | HIGH |
| **Performance Claims** | Benchmark, test result, profiling data | MEDIUM |

### 3. Citation Standards

The framework recognizes these evidence citation formats:

- **Euxis Evidence Format**: `[E1: Verified by benchmark test]`
- **Source References**: `[Source: Q4 Financial Report]`
- **Inline Citations**: `Source: Industry Analysis 2026`

## Evidence Grades

Following the Euxis Evidence & Citation Protocol:

- **E1 (VERIFIED)**: Confirmed by automated tools, tests, direct observation
- **E2 (MEASURED)**: Quantified by profiling, benchmarking, metrics
- **E3 (OBSERVED)**: Seen in source code, logs, documentation
- **E4 (INFERRED)**: Logically deduced from multiple observations

## Validation Process

### Automatic Validation

```bash
python3 evidence_validator.py <document_path>
```

The validator will:
1. Extract quantitative and strategic claims from the document
2. Check for evidence citations near each claim
3. Validate that required source types are present
4. Generate a validation report with pass/fail status

### Manual Review Requirements

Documents failing automatic validation require manual review to:
- Add missing source citations
- Upgrade speculative claims to evidence-backed assertions
- Remove or flag unsubstantiated claims

## Integration with Euxis Workflows

### Strategic Planning

All strategic planning documents must pass evidence validation before approval:

```bash
# Validate investment analysis
python3 evidence_validator.py investment_analysis.md

# Validate market research
python3 evidence_validator.py market_analysis.md
```

### Quality Gates

The evidence validator is integrated into quality gates:
- **Pre-commit hooks**: Validate changed strategic documents
- **Review process**: Reviewers must verify evidence validation passed
- **Release gates**: Strategic artifacts require evidence validation

## Compliance Requirements

### Mandatory Evidence

These claim types MUST have evidence citations:
- Financial projections and cost estimates
- Market share and competitive positioning claims
- Performance improvement assertions
- ROI calculations and business case metrics

### Severity Handling

- **CRITICAL failures**: Block document approval
- **HIGH failures**: Require reviewer override with justification
- **MEDIUM/LOW failures**: Generate warnings only

## Exemptions and Overrides

### Pre-mortem Analysis

Strategic planning documents may include speculative scenarios marked as:
```markdown
[SPECULATIVE - PRE-MORTEM ANALYSIS]
If market conditions deteriorate by 30%...
```

### Confidential Sources

When sources cannot be publicly cited:
```markdown
[E2: Measured via internal benchmark - confidential methodology]
```

## Framework Evolution

The evidence requirements are versioned and updated based on:
- Audit findings and compliance gaps
- Industry best practices
- Regulatory requirements
- Strategic planning effectiveness

## Quality Metrics

The framework tracks:
- **Evidence Coverage**: Percentage of claims with adequate evidence
- **Validation Pass Rate**: Documents passing on first validation attempt
- **Source Quality**: Distribution of evidence grades (E1-E4)
- **Compliance Trends**: Evidence quality over time

## Implementation Guidelines

### For Document Authors

1. Include source citations for all quantitative claims
2. Use recognized citation formats
3. Validate documents before submission
4. Address validation failures promptly

### For Reviewers

1. Verify evidence validation passed
2. Spot-check source quality and relevance
3. Escalate systematic evidence gaps
4. Approve overrides only with documented justification

### For Strategic Planning

1. Require evidence validation for all strategic artifacts
2. Plan evidence gathering during research phases
3. Budget time for source documentation
4. Track evidence quality as a planning metric

---

**Framework Version**: 0.0.7
**Last Updated**: 2026-02-14
**Owner**: Euxis Debugging Specialist
**Review Cycle**: Quarterly