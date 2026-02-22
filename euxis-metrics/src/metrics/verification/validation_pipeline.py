# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

#!/usr/bin/env python3
"""Validation Pipeline for Strategic Analysis Outputs.

This pipeline automatically processes strategic analysis reports and validates
all quantitative claims against required evidence documentation.

Features:
- Automated claim extraction from analysis reports
- Evidence requirement checking
- Source documentation validation
- Citation format verification
- Pipeline execution with failure handling
"""

import argparse
import json
import logging
import re
import shlex
import subprocess
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any, TextIO

from .evidence_framework import (
    Evidence,
    EvidenceFramework,
    EvidenceGrade,
    EvidenceVerificationError,
)

logger = logging.getLogger(__name__)

DEFAULT_PASS_THRESHOLD = 0.8


def _out(*values: object, sep: str = " ", end: str = "\n", stream: TextIO = sys.stdout) -> None:
    """Write output without using print()."""
    stream.write(sep.join(map(str, values)) + end)

class ValidationPipeline:
    """Pipeline for validating strategic analysis outputs."""

    def __init__(self, evidence_framework: EvidenceFramework | None = None) -> None:
        self.framework = evidence_framework or EvidenceFramework()
        self.validation_rules = self._load_validation_rules()

    def _load_validation_rules(self) -> dict[str, Any]:
        """Load validation rules configuration."""
        return {
            "required_evidence_grades": ["E1", "E2", "E3"],  # E4 allowed, E5 forbidden
            "minimum_evidence_per_claim": 1,
            "maximum_evidence_age_days": {
                "E1": 1,
                "E2": 1,
                "E3": 7,
                "E4": 30
            },
            "required_citation_patterns": [
                r"\[E[1-4]: .+\]",  # Evidence citation format
                (
                    r"(?:Observed|Measured|Verified|Inferred) "
                    r"(?:at|in|by) .+"
                ),  # Evidence description
            ],
            "forbidden_terms": [
                "approximately", "roughly", "around", "about",
                "seems", "appears", "likely", "probably",
                "I think", "maybe", "perhaps"
            ],
            "required_verification_commands": True,
            "quantitative_claim_patterns": [
                r"\b\d+\.?\d*%\b",  # Percentages
                r"\b\d+\.?\d*\s*(?:ms|milliseconds?|seconds?|minutes?|hours?)\b",  # Time
                r"\b\d+\.?\d*\s*(?:MB|GB|KB|bytes?)\b",  # Size
                r"\b\d+\.?\d*x\s+(?:faster|slower|more|less|better|worse)\b",  # Comparisons
                r"\bP\d+\s*[:\-]?\s*\d+\.?\d*\b",  # Percentiles
                r"\b(?:average|median|mean|max|min|total|sum)\s*[:\-]?\s*\d+\.?\d*\b",  # Stats
                r"\b(?:success|failure|error)\s+rate\s*[:\-]?\s*\d+\.?\d*%?\b",  # Rates
                r"\b\d+\.?\d*%\s+(?:success|failure|error)\s+rate\b",  # Percent-before-rate
                (
                    r"\b\d+\.?\d*\s*(?:requests?|operations?|tasks?)\s*per\s*"
                    r"(?:second|minute|hour)\b"
                ),  # Throughput
            ]
        }

    def extract_quantitative_claims(self, text: str) -> list[dict[str, Any]]:
        """Extract quantitative claims from analysis text."""
        claims = []
        patterns = self.validation_rules["quantitative_claim_patterns"]

        for pattern in patterns:
            matches = re.finditer(pattern, text, re.IGNORECASE)
            for match in matches:
                # Get context around the claim
                context_start = max(0, match.start() - 100)
                context_end = min(len(text), match.end() + 100)
                context = text[context_start:context_end].strip()

                # Extract the matched value
                matched_text = match.group(0)

                # Try to extract numerical value
                num_match = re.search(r"\d+\.?\d*", matched_text)
                value = float(num_match.group()) if num_match else 0

                # Determine unit
                unit_match = re.search(r"[a-zA-Z%]+", matched_text)
                unit = unit_match.group() if unit_match else ""

                claim_data = {
                    "matched_text": matched_text,
                    "value": value,
                    "unit": unit,
                    "context": context,
                    "position": {"start": match.start(), "end": match.end()},
                    "pattern_type": pattern
                }
                claims.append(claim_data)

        return claims

    def check_citation_requirements(
        self,
        _text: str,
        claims: list[dict[str, Any]],
    ) -> dict[str, Any]:
        """Check if all claims have proper citations."""
        citation_check = {
            "total_claims": len(claims),
            "cited_claims": 0,
            "uncited_claims": [],
            "citation_issues": []
        }

        citation_patterns = self.validation_rules["required_citation_patterns"]

        for i, claim in enumerate(claims):
            claim_context = claim["context"]
            cited = False

            # Check for citation patterns near the claim
            for pattern in citation_patterns:
                if re.search(pattern, claim_context, re.IGNORECASE):
                    cited = True
                    break

            if cited:
                citation_check["cited_claims"] += 1
            else:
                citation_check["uncited_claims"].append({
                    "claim_id": i,
                    "text": claim["matched_text"],
                    "context": claim["context"][:100] + "..."
                })

        return citation_check

    def check_forbidden_terms(self, text: str) -> dict[str, Any]:
        """Check for forbidden uncertainty terms."""
        forbidden = self.validation_rules["forbidden_terms"]
        found_terms = []

        for term in forbidden:
            pattern = r"\b" + re.escape(term) + r"\b"
            matches = list(re.finditer(pattern, text, re.IGNORECASE))
            if matches:
                for match in matches:
                    context_start = max(0, match.start() - 50)
                    context_end = min(len(text), match.end() + 50)
                    context = text[context_start:context_end].strip()

                    found_terms.append({
                        "term": term,
                        "position": match.start(),
                        "context": context
                    })

        return {
            "forbidden_terms_found": len(found_terms),
            "violations": found_terms
        }

    def validate_evidence_commands(self, evidence_list: list[Evidence]) -> dict[str, Any]:
        """Validate that evidence has proper verification commands."""
        validation = {
            "total_evidence": len(evidence_list),
            "with_commands": 0,
            "command_test_results": [],
            "missing_commands": []
        }

        for evidence in evidence_list:
            if evidence.verification_cmd:
                validation["with_commands"] += 1

                # Test the verification command
                try:
                    # verification_cmd is sourced from trusted evidence definitions
                    result = subprocess.run(  # noqa: S603
                        shlex.split(evidence.verification_cmd),
                        capture_output=True,
                        text=True,
                        timeout=10,
                        check=False,
                    )

                    validation["command_test_results"].append({
                        "evidence_hash": evidence.get_hash(),
                        "command": evidence.verification_cmd,
                        "success": result.returncode == 0,
                        "stdout": result.stdout[:200],
                        "stderr": result.stderr[:200]
                    })

                except subprocess.TimeoutExpired:
                    validation["command_test_results"].append({
                        "evidence_hash": evidence.get_hash(),
                        "command": evidence.verification_cmd,
                        "success": False,
                        "error": "Command timeout"
                    })
                except (OSError, subprocess.SubprocessError, ValueError) as exc:
                    validation["command_test_results"].append({
                        "evidence_hash": evidence.get_hash(),
                        "command": evidence.verification_cmd,
                        "success": False,
                        "error": str(exc)
                    })
            else:
                validation["missing_commands"].append({
                    "evidence_hash": evidence.get_hash(),
                    "evidence_type": evidence.evidence_type,
                    "source": evidence.source_file
                })

        return validation

    def process_analysis_file(self, file_path: Path) -> dict[str, Any]:
        """Process a strategic analysis file through the validation pipeline."""
        if not file_path.exists():
            msg = f"Analysis file not found: {file_path}"
            raise FileNotFoundError(msg)

        # Read the analysis text
        try:
            with file_path.open(encoding="utf-8") as f:
                if file_path.suffix.lower() == ".json":
                    data = json.load(f)
                    # Extract text from various possible fields
                    text = ""
                    for field in ["content", "analysis", "report", "summary", "text"]:
                        if field in data:
                            text += str(data[field]) + "\n"
                    if not text.strip():
                        text = json.dumps(data, indent=2)
                else:
                    text = f.read()
        except (OSError, json.JSONDecodeError) as exc:
            msg = f"Failed to read analysis file: {exc}"
            raise EvidenceVerificationError(msg) from exc

        # Run validation pipeline
        pipeline_result = {
            "file_path": str(file_path),
            "timestamp": datetime.now(UTC).isoformat(),
            "file_size": file_path.stat().st_size,
            "validation_steps": {}
        }

        # Step 1: Extract quantitative claims
        claims_data = self.extract_quantitative_claims(text)
        pipeline_result["validation_steps"]["claim_extraction"] = {
            "claims_found": len(claims_data),
            "claims": claims_data
        }

        # Step 2: Check citation requirements
        citation_check = self.check_citation_requirements(text, claims_data)
        pipeline_result["validation_steps"]["citation_check"] = citation_check

        # Step 3: Check for forbidden terms
        forbidden_check = self.check_forbidden_terms(text)
        pipeline_result["validation_steps"]["forbidden_terms"] = forbidden_check

        # Step 4: Extract embedded evidence (if any JSON evidence is embedded)
        embedded_evidence = self._extract_embedded_evidence(text)
        if embedded_evidence:
            evidence_validation = self.validate_evidence_commands(embedded_evidence)
            pipeline_result["validation_steps"]["evidence_validation"] = evidence_validation

        # Calculate overall validation score
        score = self._calculate_validation_score(pipeline_result)
        pipeline_result["overall_score"] = score
        pipeline_result["passed"] = score >= DEFAULT_PASS_THRESHOLD

        return pipeline_result

    def _extract_embedded_evidence(self, text: str) -> list[Evidence]:
        """Extract evidence objects from embedded JSON in text."""
        evidence_list = []

        # Look for JSON blocks that might contain evidence
        json_blocks = re.findall(r'\{[^}]*"evidence"[^}]*\}', text, re.DOTALL)

        for block in json_blocks:
            try:
                data = json.loads(block)
                if "evidence" in data:
                    # Convert to Evidence objects
                    for ev_data in data["evidence"]:
                        if isinstance(ev_data, dict):
                            evidence = Evidence(
                                source_file=ev_data.get("source_file", "unknown"),
                                source_line=ev_data.get("source_line"),
                                evidence_type=ev_data.get("evidence_type", "unknown"),
                                grade=EvidenceGrade(ev_data.get("grade", "E4")),
                                content=ev_data.get("content", ""),
                                timestamp=datetime.now(UTC),
                                verification_cmd=ev_data.get("verification_cmd"),
                                metadata=ev_data.get("metadata", {})
                            )
                            evidence_list.append(evidence)
            except (json.JSONDecodeError, ValueError, KeyError):
                continue

        return evidence_list

    def _calculate_validation_score(self, pipeline_result: dict[str, Any]) -> float:
        """Calculate overall validation score from pipeline results."""
        score = 1.0

        # Citation compliance (40% of score)
        citation_data = pipeline_result["validation_steps"]["citation_check"]
        if citation_data["total_claims"] > 0:
            citation_ratio = citation_data["cited_claims"] / citation_data["total_claims"]
            score *= (0.6 + 0.4 * citation_ratio)  # Min 60%, max 100%

        # Forbidden terms penalty (20% of score)
        forbidden_data = pipeline_result["validation_steps"]["forbidden_terms"]
        if forbidden_data["forbidden_terms_found"] > 0:
            penalty = min(0.2, forbidden_data["forbidden_terms_found"] * 0.05)
            score -= penalty

        # Evidence validation (40% of score, if evidence present)
        if "evidence_validation" in pipeline_result["validation_steps"]:
            ev_data = pipeline_result["validation_steps"]["evidence_validation"]
            if ev_data["total_evidence"] > 0:
                command_ratio = ev_data["with_commands"] / ev_data["total_evidence"]
                success_ratio = 0
                if ev_data["command_test_results"]:
                    successful_tests = sum(
                        1
                        for r in ev_data["command_test_results"]
                        if r["success"]
                    )
                    success_ratio = successful_tests / len(ev_data["command_test_results"])

                evidence_score = (command_ratio * 0.5) + (success_ratio * 0.5)
                score = score * 0.6 + evidence_score * 0.4

        return max(0.0, min(1.0, score))

    def generate_validation_report(
        self,
        pipeline_results: list[dict[str, Any]],
    ) -> dict[str, Any]:
        """Generate comprehensive validation report from multiple pipeline results."""
        report = {
            "report_timestamp": datetime.now(UTC).isoformat(),
            "files_analyzed": len(pipeline_results),
            "summary": {
                "files_passed": sum(1 for r in pipeline_results if r["passed"]),
                "files_failed": sum(1 for r in pipeline_results if not r["passed"]),
                "average_score": (
                    sum(r["overall_score"] for r in pipeline_results)
                    / len(pipeline_results)
                    if pipeline_results
                    else 0
                ),
                "total_claims": sum(
                    r["validation_steps"]["claim_extraction"]["claims_found"]
                    for r in pipeline_results
                ),
                "total_citations": sum(
                    r["validation_steps"]["citation_check"]["cited_claims"]
                    for r in pipeline_results
                ),
                "forbidden_violations": sum(
                    r["validation_steps"]["forbidden_terms"]["forbidden_terms_found"]
                    for r in pipeline_results
                ),
            },
            "file_results": pipeline_results,
            "recommendations": []
        }

        # Generate recommendations
        if report["summary"]["files_failed"] > 0:
            message = (
                "Fix "
                f"{report['summary']['files_failed']} "
                "failed validation files before publication"
            )
            report["recommendations"].append(message)

        if report["summary"]["total_citations"] < report["summary"]["total_claims"]:
            missing_citations = (
                report["summary"]["total_claims"]
                - report["summary"]["total_citations"]
            )
            report["recommendations"].append(
                f"Add {missing_citations} missing evidence citations"
            )

        if report["summary"]["forbidden_violations"] > 0:
            message = (
                "Remove "
                f"{report['summary']['forbidden_violations']} "
                "instances of forbidden uncertainty language"
            )
            report["recommendations"].append(message)

        return report

def main() -> None:
    """CLI interface for the validation pipeline."""
    parser = argparse.ArgumentParser(
        description="Evidence Verification Pipeline for Strategic Analysis"
    )
    parser.add_argument("files", nargs="+", help="Analysis files to validate")
    parser.add_argument("--output", "-o", help="Output validation report to file")
    parser.add_argument("--threshold", type=float, default=0.8, help="Validation score threshold")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")

    args = parser.parse_args()

    # Setup logging
    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=log_level, format="%(levelname)s: %(message)s")

    pipeline = ValidationPipeline()
    results = []

    for file_path in args.files:
        path = Path(file_path)
        try:
            result = pipeline.process_analysis_file(path)
            results.append(result)

            if args.verbose:
                _out(f"\nValidated: {file_path}")
                _out(f"Score: {result['overall_score']:.2f}")
                _out(f"Passed: {'YES' if result['passed'] else 'NO'}")

        except (
            OSError,
            ValueError,
            EvidenceVerificationError,
            subprocess.CalledProcessError,
        ) as exc:
            logger.exception("Failed to validate %s", file_path)
            results.append({
                "file_path": str(path),
                "error": str(exc),
                "overall_score": 0.0,
                "passed": False
            })

    # Generate final report
    report = pipeline.generate_validation_report(results)

    if args.output:
        with Path(args.output).open("w") as f:
            json.dump(report, f, indent=2)
        _out(f"Validation report saved to: {args.output}")
    else:
        _out(json.dumps(report, indent=2))

    # Exit with non-zero if validations failed
    failed_count = report["summary"]["files_failed"]
    if failed_count > 0:
        _out(f"\nValidation FAILED: {failed_count} files did not meet requirements")
        sys.exit(1)
    else:
        _out(f"\nValidation PASSED: All {len(results)} files met requirements")
        sys.exit(0)

if __name__ == "__main__":
    main()
