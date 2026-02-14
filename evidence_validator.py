#!/usr/bin/env python3
"""
Evidence Validation Framework for Strategic Analysis Outputs
Validates that quantitative claims are backed by source documentation
"""

import re
import sys
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass


@dataclass
class EvidenceRequirement:
    """Defines what evidence is required for different claim types"""
    claim_pattern: str
    required_sources: List[str]
    severity: str  # CRITICAL, HIGH, MEDIUM, LOW


@dataclass
class ValidationResult:
    """Result of evidence validation"""
    claim: str
    has_evidence: bool
    sources_found: List[str]
    missing_sources: List[str]
    severity: str


class EvidenceValidator:
    """Validates evidence backing for quantitative and strategic claims"""

    def __init__(self):
        self.requirements = [
            EvidenceRequirement(
                claim_pattern=r'\d+%|percentage|percent',
                required_sources=['measurement', 'benchmark', 'survey', 'study'],
                severity='HIGH'
            ),
            EvidenceRequirement(
                claim_pattern=r'\$\d+|\d+\s*(million|billion|thousand)',
                required_sources=['financial_report', 'budget', 'invoice', 'estimate'],
                severity='CRITICAL'
            ),
            EvidenceRequirement(
                claim_pattern=r'market\s+share|competitive\s+advantage',
                required_sources=['market_research', 'competitor_analysis', 'industry_report'],
                severity='HIGH'
            ),
            EvidenceRequirement(
                claim_pattern=r'performance\s+improvement|\d+x\s+faster|latency|throughput',
                required_sources=['benchmark', 'test_result', 'profiling_data'],
                severity='MEDIUM'
            ),
        ]

    def extract_claims(self, content: str) -> List[str]:
        """Extract quantitative and strategic claims from content"""
        claims = []
        lines = content.split('\n')

        for line in lines:
            for req in self.requirements:
                if re.search(req.claim_pattern, line, re.IGNORECASE):
                    claims.append(line.strip())
                    break

        return claims

    def find_evidence_sources(self, content: str, claim: str) -> List[str]:
        """Find evidence sources referenced near a claim"""
        sources = []

        # Look for citation patterns
        citation_patterns = [
            r'\[E\d+:\s*([^\]]+)\]',  # Euxis evidence format
            r'\[[^\]]*source[^\]]*:\s*([^\]]+)\]',
            r'\[[^\]]*ref[^\]]*:\s*([^\]]+)\]',
            r'Source:\s*([^\n]+)',
            r'Reference:\s*([^\n]+)',
        ]

        for pattern in citation_patterns:
            matches = re.findall(pattern, content, re.IGNORECASE)
            sources.extend(matches)

        return sources

    def validate_claim(self, content: str, claim: str) -> ValidationResult:
        """Validate evidence backing for a specific claim"""
        # Find which requirement this claim matches
        matching_req = None
        for req in self.requirements:
            if re.search(req.claim_pattern, claim, re.IGNORECASE):
                matching_req = req
                break

        if not matching_req:
            return ValidationResult(
                claim=claim,
                has_evidence=True,  # No specific requirements
                sources_found=[],
                missing_sources=[],
                severity='LOW'
            )

        # Find evidence sources
        sources_found = self.find_evidence_sources(content, claim)

        # Check if required source types are present
        missing_sources = []
        for required in matching_req.required_sources:
            source_found = any(required.lower() in src.lower() for src in sources_found)
            if not source_found:
                missing_sources.append(required)

        has_evidence = len(missing_sources) == 0

        return ValidationResult(
            claim=claim,
            has_evidence=has_evidence,
            sources_found=sources_found,
            missing_sources=missing_sources,
            severity=matching_req.severity
        )

    def validate_document(self, file_path: str) -> Dict[str, List[ValidationResult]]:
        """Validate evidence for all claims in a document"""
        try:
            content = Path(file_path).read_text()
        except Exception as e:
            return {'error': [f"Could not read file: {e}"]}

        claims = self.extract_claims(content)
        results = []

        for claim in claims:
            result = self.validate_claim(content, claim)
            results.append(result)

        # Group results by validation status
        passed = [r for r in results if r.has_evidence]
        failed = [r for r in results if not r.has_evidence]

        return {
            'passed': passed,
            'failed': failed,
            'total_claims': len(claims)
        }

    def generate_report(self, validation_results: Dict[str, List[ValidationResult]]) -> str:
        """Generate a human-readable validation report"""
        if 'error' in validation_results:
            return f"VALIDATION ERROR: {validation_results['error'][0]}"

        report = []
        report.append("# Evidence Validation Report\n")

        total = validation_results['total_claims']
        passed = len(validation_results['passed'])
        failed = len(validation_results['failed'])

        report.append(f"## Summary")
        report.append(f"- Total claims analyzed: {total}")
        report.append(f"- Claims with adequate evidence: {passed}")
        report.append(f"- Claims missing evidence: {failed}")
        report.append(f"- Validation score: {(passed/total*100):.1f}%" if total > 0 else "- No claims found")
        report.append("")

        if validation_results['failed']:
            report.append("## Failed Validations")
            for result in validation_results['failed']:
                report.append(f"### {result.severity}: {result.claim}")
                report.append(f"**Missing sources:** {', '.join(result.missing_sources)}")
                if result.sources_found:
                    report.append(f"**Sources found:** {', '.join(result.sources_found)}")
                report.append("")

        if validation_results['passed']:
            report.append("## Passed Validations")
            for result in validation_results['passed']:
                report.append(f"- ✓ {result.claim}")

        return '\n'.join(report)


def main():
    """Command line interface for evidence validation"""
    if len(sys.argv) != 2:
        print("Usage: python3 evidence_validator.py <file_path>")
        sys.exit(1)

    file_path = sys.argv[1]
    validator = EvidenceValidator()
    results = validator.validate_document(file_path)
    report = validator.generate_report(results)

    print(report)

    # Exit with error code if validation failed
    if 'failed' in results and results['failed']:
        sys.exit(1)


if __name__ == '__main__':
    main()