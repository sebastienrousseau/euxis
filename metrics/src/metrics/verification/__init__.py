"""Evidence Verification Framework for Euxis Strategic Analysis.

This module provides comprehensive evidence verification capabilities for strategic
analysis outputs, ensuring all quantitative claims are backed by verifiable sources.

Key Components:
- EvidenceFramework: Core evidence storage and verification
- ValidationPipeline: Automated validation of analysis reports
- Evidence/Claim models: Data structures for evidence and claims
- CLI tool: euxis-evidence-verify command-line interface

Usage:
    from metrics.verification import EvidenceFramework, ValidationPipeline

    # Basic evidence verification
    framework = EvidenceFramework()
    evidence = framework.create_evidence_from_measurement(
        measurement_file="metrics.json",
        measurement_value=95.5,
        measurement_unit="percent",
        verification_cmd="verify_success_rate.sh"
    )
    framework.store_evidence(evidence)

    # Validate analysis reports
    pipeline = ValidationPipeline(framework)
    result = pipeline.process_analysis_file(Path("analysis_report.json"))
"""

from .evidence_framework import (
    Claim,
    Evidence,
    EvidenceFramework,
    EvidenceGrade,
    EvidenceVerificationError,
    VerifiedPerformanceAnalyzer,
    create_performance_claim_with_evidence,
)
from .validation_pipeline import ValidationPipeline

__version__ = "0.0.8"
__all__ = [
    "Claim",
    "Evidence",
    "EvidenceFramework",
    "EvidenceGrade",
    "EvidenceVerificationError",
    "ValidationPipeline",
    "VerifiedPerformanceAnalyzer",
    "create_performance_claim_with_evidence"
]
