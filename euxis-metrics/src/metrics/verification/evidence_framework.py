# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

#!/usr/bin/env python3
"""Evidence Verification Framework for Strategic Analysis Outputs.

This framework validates quantitative claims in strategic analysis reports by requiring
source documentation and verifiable evidence for all assertions.

Key features:
- Evidence grading (E1-E5 as per Euxis Protocol)
- Source documentation tracking
- Claim verification pipeline
- Automated citation validation
- Evidence decay detection
"""

import hashlib
import json
import logging
import os
import re
import shlex
import subprocess
from dataclasses import asdict, dataclass
from datetime import UTC, datetime, timedelta
from enum import Enum
from pathlib import Path
from typing import Any

from metrics.aggregators.performance_analyzer import PerformanceAnalyzer

logger = logging.getLogger(__name__)

MAX_ALLOWED_GRADE_INDEX = 2

class EvidenceGrade(Enum):
    """Evidence grade hierarchy as defined in Euxis Evidence Citation Protocol."""

    # Confirmed by automated tool output, test result, or direct observation
    VERIFIED = "E1"
    # Quantified by profiling, benchmarking, or metric collection
    MEASURED = "E2"
    # Seen in source code, logs, or documentation but not independently verified
    OBSERVED = "E3"
    # Logically deduced from multiple observations, not directly confirmed
    INFERRED = "E4"
    # FORBIDDEN - Guesses, hunches, or assumptions without supporting observations
    SPECULATED = "E5"

@dataclass
class Evidence:
    """Represents a piece of evidence supporting a claim."""

    source_file: str
    source_line: int | None
    evidence_type: str  # "measurement", "observation", "test_result", "log_entry", etc.
    grade: EvidenceGrade
    content: str
    timestamp: datetime
    verification_cmd: str | None  # Command that can re-verify this evidence
    metadata: dict[str, Any]

    def get_hash(self) -> str:
        """Generate unique hash for this evidence."""
        content_str = (
            f"{self.source_file}:{self.source_line}:{self.content}:{self.timestamp.isoformat()}"
        )
        return hashlib.sha256(content_str.encode()).hexdigest()[:12]

@dataclass
class Claim:
    """Represents a quantitative claim in a strategic analysis."""

    statement: str
    value: float | int | str
    unit: str | None
    context: str
    supporting_evidence: list[Evidence]
    confidence: float  # 0.0 - 1.0
    verified: bool = False

    def get_highest_evidence_grade(self) -> EvidenceGrade | None:
        """Get the highest grade evidence supporting this claim."""
        if not self.supporting_evidence:
            return None
        grades = [e.grade for e in self.supporting_evidence]
        # Lower enum values mean higher grades (E1 > E2 > E3 > E4)
        return min(grades, key=lambda x: list(EvidenceGrade).index(x))

class EvidenceVerificationError(Exception):
    """Raised when evidence verification fails."""


class EvidenceFramework:
    """Main evidence verification framework."""

    def __init__(self, evidence_dir: str | None = None) -> None:
        base = evidence_dir or str(
            Path(os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis"))) / "euxis-runtime" / "metrics" / "evidence"
        )
        self.evidence_dir = Path(base)
        self.evidence_dir.mkdir(parents=True, exist_ok=True)

        self.evidence_db = self.evidence_dir / "evidence.jsonl"
        self.verification_log = self.evidence_dir / "verification.log"

        # Setup logging
        logging.basicConfig(
            filename=str(self.verification_log),
            level=logging.INFO,
            format="%(asctime)s - %(levelname)s - %(message)s"
        )

        # Evidence decay thresholds (in days)
        self.decay_thresholds = {
            EvidenceGrade.VERIFIED: 1,    # Re-verify E1 evidence after 1 day
            EvidenceGrade.MEASURED: 1,    # Re-verify E2 evidence after 1 day
            EvidenceGrade.OBSERVED: 7,    # Re-verify E3 evidence after 7 days
            EvidenceGrade.INFERRED: 30    # Re-verify E4 evidence after 30 days
        }

    def store_evidence(self, evidence: Evidence) -> str:
        """Store evidence in the database."""
        evidence_dict = asdict(evidence)
        evidence_dict["grade"] = evidence.grade.value
        evidence_dict["timestamp"] = evidence.timestamp.isoformat()
        evidence_dict["hash"] = evidence.get_hash()

        # Append to JSONL database
        with self.evidence_db.open("a") as f:
            f.write(json.dumps(evidence_dict) + "\n")

        logger.info("Stored evidence: %s", evidence.get_hash())
        return evidence.get_hash()

    def load_evidence(self, evidence_hash: str) -> Evidence | None:
        """Load evidence by hash from database."""
        if not self.evidence_db.exists():
            return None

        with self.evidence_db.open() as f:
            for line in f:
                try:
                    data = json.loads(line.strip())
                    if data.get("hash") == evidence_hash:
                        # Reconstruct Evidence object
                        return Evidence(
                            source_file=data["source_file"],
                            source_line=data.get("source_line"),
                            evidence_type=data["evidence_type"],
                            grade=EvidenceGrade(data["grade"]),
                            content=data["content"],
                            timestamp=datetime.fromisoformat(data["timestamp"]),
                            verification_cmd=data.get("verification_cmd"),
                            metadata=data.get("metadata", {}),
                        )
                except (json.JSONDecodeError, KeyError, ValueError):
                    continue
        return None

    def verify_evidence(self, evidence: Evidence) -> bool:
        """Verify evidence using its verification command."""
        if not evidence.verification_cmd:
            logger.warning("No verification command for evidence %s", evidence.get_hash())
            return False

        try:
            # verification_cmd is sourced from trusted evidence definitions
            if re.search(r"[;&|><()$`]", evidence.verification_cmd):
                result = subprocess.run(  # noqa: S603
                    evidence.verification_cmd,
                    capture_output=True,
                    text=True,
                    timeout=30,
                    check=False,
                    shell=True,
                )
            else:
                result = subprocess.run(  # noqa: S603
                    shlex.split(evidence.verification_cmd),
                    capture_output=True,
                    text=True,
                    timeout=30,
                    check=False,
                )

            success = result.returncode == 0
            logger.info(
                "Evidence verification %s: %s",
                evidence.get_hash(),
                "PASS" if success else "FAIL",
            )
        except subprocess.TimeoutExpired:
            logger.exception("Evidence verification timeout: %s", evidence.get_hash())
            return False
        except (OSError, subprocess.SubprocessError, ValueError):
            logger.exception("Evidence verification error")
            return False
        else:
            return success

    def check_evidence_decay(self, evidence: Evidence) -> bool:
        """Check if evidence has decayed beyond acceptable threshold."""
        threshold_days = self.decay_thresholds.get(evidence.grade, 7)
        decay_threshold = timedelta(days=threshold_days)

        age = datetime.now(UTC) - evidence.timestamp.replace(tzinfo=UTC)
        return age > decay_threshold

    def validate_claim(self, claim: Claim) -> dict[str, Any]:
        """Validate a claim against its supporting evidence."""
        validation_result = {
            "claim": claim.statement,
            "value": claim.value,
            "valid": True,
            "issues": [],
            "evidence_summary": {
                "total_evidence": len(claim.supporting_evidence),
                "evidence_grades": {},
                "verified_evidence": 0,
                "decayed_evidence": 0,
                "missing_verification": 0
            }
        }

        if not claim.supporting_evidence:
            validation_result["valid"] = False
            validation_result["issues"].append("No supporting evidence provided")
            return validation_result

        # Check for forbidden E5 evidence
        e5_evidence = [
            evidence
            for evidence in claim.supporting_evidence
            if evidence.grade == EvidenceGrade.SPECULATED
        ]
        if e5_evidence:
            validation_result["valid"] = False
            validation_result["issues"].append(
                f"Contains {len(e5_evidence)} forbidden E5 (SPECULATED) evidence"
            )

        # Analyze evidence grades
        grade_counts = {}
        for evidence in claim.supporting_evidence:
            grade = evidence.grade.value
            grade_counts[grade] = grade_counts.get(grade, 0) + 1

            # Check for decay
            if self.check_evidence_decay(evidence):
                validation_result["evidence_summary"]["decayed_evidence"] += 1
                validation_result["issues"].append(f"Decayed evidence: {evidence.get_hash()}")

            # Verify if possible
            if evidence.verification_cmd:
                if self.verify_evidence(evidence):
                    validation_result["evidence_summary"]["verified_evidence"] += 1
                else:
                    validation_result["valid"] = False
                    validation_result["issues"].append(
                        f"Failed verification: {evidence.get_hash()}"
                    )
            else:
                validation_result["evidence_summary"]["missing_verification"] += 1

        validation_result["evidence_summary"]["evidence_grades"] = grade_counts

        # Require at least E3 evidence for strategic claims
        highest_grade = claim.get_highest_evidence_grade()
        if (
            not highest_grade
            or list(EvidenceGrade).index(highest_grade) > MAX_ALLOWED_GRADE_INDEX
        ):
            validation_result["valid"] = False
            validation_result["issues"].append("Strategic claims require at least E3 evidence")

        return validation_result

    def validate_analysis_report(
        self,
        report: dict[str, Any],
        claims: list[Claim],
    ) -> dict[str, Any]:
        """Validate an entire strategic analysis report."""
        report_validation = {
            "report_id": report.get("report_id", "unknown"),
            "timestamp": datetime.now(UTC).isoformat(),
            "overall_valid": True,
            "claims_total": len(claims),
            "claims_valid": 0,
            "claims_invalid": 0,
            "issues": [],
            "claim_validations": []
        }

        for i, claim in enumerate(claims):
            claim_validation = self.validate_claim(claim)
            claim_validation["claim_id"] = i

            if claim_validation["valid"]:
                report_validation["claims_valid"] += 1
            else:
                report_validation["claims_invalid"] += 1
                report_validation["overall_valid"] = False

            report_validation["claim_validations"].append(claim_validation)

        # Check for citation requirements
        total_issues = sum(len(cv["issues"]) for cv in report_validation["claim_validations"])
        if total_issues > 0:
            report_validation["issues"].append(f"Total evidence issues found: {total_issues}")

        # Check evidence distribution
        all_evidence = [e for claim in claims for e in claim.supporting_evidence]
        if len(all_evidence) < len(claims):
            report_validation["issues"].append(
                "Insufficient evidence density (< 1 evidence per claim)"
            )

        return report_validation

    def generate_citation_format(self, evidence: Evidence) -> str:
        """Generate properly formatted citation for evidence."""
        grade = evidence.grade.value
        source_ref = evidence.source_file
        if evidence.source_line:
            source_ref += f":{evidence.source_line}"

        return f"[{grade}: {evidence.evidence_type} in {source_ref}]"

    def extract_claims_from_text(self, text: str) -> list[str]:
        """Extract quantitative claims from analysis text."""
        # Patterns for quantitative claims
        patterns = [
            r"\b\d+\.?\d*%\b",  # Percentages
            r"\b\d+\.?\d*\s*(ms|seconds?|minutes?|hours?)\b",  # Time measurements
            r"\brate\s+of\s+\d+\.?\d*\b",  # Rates
            r"\b\d+\.?\d*x\s+(?:faster|slower|more|less)\b",  # Comparisons
            r"\bP\d+\s+\d+\.?\d*\b",  # Percentiles
            r"\b(?:average|median|mean)\s+\d+\.?\d*\b",  # Statistical measures
        ]

        claims = []
        for pattern in patterns:
            matches = re.finditer(pattern, text, re.IGNORECASE)
            for match in matches:
                context_start = max(0, match.start() - 50)
                context_end = min(len(text), match.end() + 50)
                context = text[context_start:context_end].strip()
                claims.append(context)

        return claims

    def create_evidence_from_measurement(
        self,
        measurement_file: str,
        measurement_value: float,
        measurement_unit: str,
        verification_cmd: str
    ) -> Evidence:
        """Create E2 (MEASURED) evidence from a measurement."""
        return Evidence(
            source_file=measurement_file,
            source_line=None,
            evidence_type="measurement",
            grade=EvidenceGrade.MEASURED,
            content=f"{measurement_value} {measurement_unit}",
            timestamp=datetime.now(UTC),
            verification_cmd=verification_cmd,
            metadata={"measurement_value": measurement_value, "unit": measurement_unit}
        )

    def create_evidence_from_observation(
        self,
        source_file: str,
        source_line: int,
        observation: str
    ) -> Evidence:
        """Create E3 (OBSERVED) evidence from code/log observation."""
        return Evidence(
            source_file=source_file,
            source_line=source_line,
            evidence_type="code_observation",
            grade=EvidenceGrade.OBSERVED,
            content=observation,
            timestamp=datetime.now(UTC),
            verification_cmd=f"grep -n '{observation}' {source_file}",
            metadata={}
        )

def create_performance_claim_with_evidence(
    claim_statement: str,
    measured_value: float,
    unit: str,
    measurement_source: str,
    verification_cmd: str
) -> Claim:
    """Create performance claims with proper evidence."""
    evidence = Evidence(
        source_file=measurement_source,
        source_line=None,
        evidence_type="performance_measurement",
        grade=EvidenceGrade.MEASURED,
        content=f"Measured: {measured_value} {unit}",
        timestamp=datetime.now(UTC),
        verification_cmd=verification_cmd,
        metadata={"value": measured_value, "unit": unit}
    )

    return Claim(
        statement=claim_statement,
        value=measured_value,
        unit=unit,
        context="Performance analysis",
        supporting_evidence=[evidence],
        confidence=0.95
    )

# Integration with existing PerformanceAnalyzer
class VerifiedPerformanceAnalyzer:
    """Extended PerformanceAnalyzer with evidence verification."""

    def __init__(self, metrics_dir: str | None = None) -> None:
        base = metrics_dir or str(Path(os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis"))) / "euxis-runtime" / "metrics")
        self.analyzer = PerformanceAnalyzer(base)
        self.evidence_framework = EvidenceFramework()

    def generate_verified_report(self, hours_back: int = 24) -> dict[str, Any]:
        """Generate performance report with evidence verification."""
        # Get base report
        base_report = self.analyzer.generate_performance_report(hours_back)

        # Extract and verify claims
        claims = []

        # Process fleet metrics
        if "fleet_metrics" in base_report:
            fleet = base_report["fleet_metrics"]

            # Success rate claim
            if "fleet_success_rate" in fleet:
                success_rate = fleet["fleet_success_rate"] * 100  # Convert to percentage
                verification_cmd = (
                    'python3 -c "import json; '
                    f"sessions=[json.loads(l) for l in open('{self.analyzer.sessions_file}')]; "
                    "successes=sum(1 for s in sessions if s['status']=='SUCCESS'); "
                    "print(f'Success rate: {successes/len(sessions)*100:.1f}%')\""
                )
                evidence = self.evidence_framework.create_evidence_from_measurement(
                    measurement_file=str(self.analyzer.sessions_file),
                    measurement_value=success_rate,
                    measurement_unit="percent",
                    verification_cmd=verification_cmd,
                )

                claim = Claim(
                    statement=f"Fleet success rate is {success_rate:.1f}%",
                    value=success_rate,
                    unit="percent",
                    context="Fleet-wide performance analysis",
                    supporting_evidence=[evidence],
                    confidence=0.99
                )
                claims.append(claim)
                self.evidence_framework.store_evidence(evidence)

        # Validate all claims
        validation_report = self.evidence_framework.validate_analysis_report(base_report, claims)

        # Enhance base report with validation
        base_report["evidence_validation"] = validation_report
        base_report["verified"] = validation_report["overall_valid"]

        return base_report

if __name__ == "__main__":
    # Example usage
    framework = EvidenceFramework()

    # Create sample evidence
    evidence = framework.create_evidence_from_measurement(
        measurement_file=str(
            Path(os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis")))
            / "metrics"
            / "sessions.jsonl"
        ),
        measurement_value=95.5,
        measurement_unit="percent",
        verification_cmd="echo 'Mock verification command'"
    )

    # Create sample claim
    claim = Claim(
        statement="System success rate is 95.5%",
        value=95.5,
        unit="percent",
        context="Performance analysis",
        supporting_evidence=[evidence],
        confidence=0.95
    )

    # Validate claim
    validation = framework.validate_claim(claim)
    logger.info("Validation result: %s", json.dumps(validation, indent=2))
