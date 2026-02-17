# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

#!/usr/bin/env python3
"""Integration tests for the Evidence Verification Framework.

Tests the complete evidence verification pipeline including:
- Evidence storage and retrieval
- Claim validation against evidence
- Validation pipeline processing
- CLI interface functionality
"""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from datetime import UTC, datetime, timedelta
from pathlib import Path

import pytest

# Add metrics src to path
metrics_src = Path(__file__).resolve().parent.parent / "src"
sys.path.insert(0, str(metrics_src))

from metrics.verification import Claim, Evidence, EvidenceFramework, EvidenceGrade, ValidationPipeline


def _out(*values: object, sep: str = " ", end: str = "\n") -> None:
    """Write output without using print()."""
    sys.stdout.write(sep.join(map(str, values)) + end)


class TestEvidenceFramework(unittest.TestCase):
    """Test the core evidence framework functionality."""

    def setUp(self):
        """Set up test environment with temporary directories."""
        self.temp_dir = tempfile.mkdtemp()
        self.framework = EvidenceFramework(evidence_dir=self.temp_dir)

    def tearDown(self):
        """Cleanup temporary files."""
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_evidence_creation_and_storage(self):
        """Test creating and storing evidence."""
        evidence = Evidence(
            source_file="/test/metrics.json",
            source_line=42,
            evidence_type="performance_measurement",
            grade=EvidenceGrade.MEASURED,
            content="Response time: 150ms",
            timestamp=datetime.now(UTC),
            verification_cmd="echo 'test verification'",
            metadata={"response_time": 150, "unit": "ms"}
        )

        # Store evidence
        evidence_hash = self.framework.store_evidence(evidence)
        assert evidence_hash is not None
        assert len(evidence_hash) == 12  # Hash should be 12 characters

        # Retrieve evidence
        retrieved = self.framework.load_evidence(evidence_hash)
        assert retrieved is not None
        assert retrieved.source_file == evidence.source_file
        assert retrieved.grade == evidence.grade

    def test_evidence_verification(self):
        """Test evidence verification using commands."""
        # Create evidence with a command that will succeed
        evidence = Evidence(
            source_file="/test/success.txt",
            source_line=1,
            evidence_type="test_result",
            grade=EvidenceGrade.VERIFIED,
            content="Test passed",
            timestamp=datetime.now(UTC),
            verification_cmd="echo 'success' && exit 0",
            metadata={}
        )

        result = self.framework.verify_evidence(evidence)
        assert result is True

        # Create evidence with a command that will fail
        failing_evidence = Evidence(
            source_file="/test/failure.txt",
            source_line=1,
            evidence_type="test_result",
            grade=EvidenceGrade.VERIFIED,
            content="Test failed",
            timestamp=datetime.now(UTC),
            verification_cmd="echo 'failure' && exit 1",
            metadata={}
        )

        result = self.framework.verify_evidence(failing_evidence)
        assert result is False

    def test_evidence_decay_checking(self):
        """Test evidence decay detection."""
        # Create old evidence that should be decayed
        old_evidence = Evidence(
            source_file="/test/old.txt",
            source_line=1,
            evidence_type="measurement",
            grade=EvidenceGrade.VERIFIED,  # Should decay after 1 day
            content="Old measurement",
            timestamp=datetime.now(UTC) - timedelta(days=2),
            verification_cmd=None,
            metadata={}
        )

        is_decayed = self.framework.check_evidence_decay(old_evidence)
        assert is_decayed is True

        # Create fresh evidence that should not be decayed
        fresh_evidence = Evidence(
            source_file="/test/fresh.txt",
            source_line=1,
            evidence_type="measurement",
            grade=EvidenceGrade.VERIFIED,
            content="Fresh measurement",
            timestamp=datetime.now(UTC) - timedelta(hours=1),
            verification_cmd=None,
            metadata={}
        )

        is_decayed = self.framework.check_evidence_decay(fresh_evidence)
        assert is_decayed is False

    def test_claim_validation(self):
        """Test claim validation against evidence."""
        # Create valid evidence
        evidence = Evidence(
            source_file="/test/metrics.json",
            source_line=100,
            evidence_type="performance_measurement",
            grade=EvidenceGrade.MEASURED,
            content="Success rate: 95.5%",
            timestamp=datetime.now(UTC),
            verification_cmd="echo 'verified' && exit 0",
            metadata={"success_rate": 95.5}
        )

        # Create claim with evidence
        claim = Claim(
            statement="System success rate is 95.5%",
            value=95.5,
            unit="percent",
            context="Performance analysis",
            supporting_evidence=[evidence],
            confidence=0.95
        )

        validation = self.framework.validate_claim(claim)
        assert validation["valid"] is True
        assert validation["evidence_summary"]["total_evidence"] == 1

    def test_claim_validation_forbidden_evidence(self):
        """Test that claims with E5 evidence are rejected."""
        # Create forbidden E5 evidence
        forbidden_evidence = Evidence(
            source_file="/test/speculation.txt",
            source_line=1,
            evidence_type="speculation",
            grade=EvidenceGrade.SPECULATED,  # E5 - forbidden
            content="I think the success rate is around 95%",
            timestamp=datetime.now(UTC),
            verification_cmd=None,
            metadata={}
        )

        claim = Claim(
            statement="System success rate might be around 95%",
            value=95.0,
            unit="percent",
            context="Speculative analysis",
            supporting_evidence=[forbidden_evidence],
            confidence=0.5
        )

        validation = self.framework.validate_claim(claim)
        assert validation["valid"] is False
        assert any("E5" in issue for issue in validation["issues"])

class TestValidationPipeline(unittest.TestCase):
    """Test the validation pipeline functionality."""

    def setUp(self):
        """Set up test environment."""
        self.temp_dir = tempfile.mkdtemp()
        self.framework = EvidenceFramework(evidence_dir=self.temp_dir)
        self.pipeline = ValidationPipeline(self.framework)

    def tearDown(self):
        """Cleanup temporary files."""
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_quantitative_claim_extraction(self):
        """Test extraction of quantitative claims from text."""
        analysis_text = """
        Performance Analysis Report

        The system achieved a 95.5% success rate during the test period.
        Average response time was 150ms with P95 latency of 300ms.
        The system processed 1000 requests per second at peak load.
        Memory usage averaged 2.5GB throughout the test.
        """

        claims = self.pipeline.extract_quantitative_claims(analysis_text)
        assert len(claims) > 0

        # Check for specific claims
        claim_texts = [claim["matched_text"] for claim in claims]
        assert any("95.5%" in text for text in claim_texts)
        assert any("150ms" in text for text in claim_texts)
        assert any("300ms" in text for text in claim_texts)

    def test_citation_requirement_checking(self):
        """Test checking for proper evidence citations."""
        # Text with proper citations
        cited_text = """
        The system success rate is 95.5% [E2: Measured via load test #847].
        Response time averaged 150ms [E1: Verified by monitoring dashboard].
        """

        claims = self.pipeline.extract_quantitative_claims(cited_text)
        citation_check = self.pipeline.check_citation_requirements(cited_text, claims)

        assert citation_check["cited_claims"] == len(claims)
        assert len(citation_check["uncited_claims"]) == 0

        # Text without proper citations
        uncited_text = """
        The system success rate is 95.5% based on our testing.
        Response time averaged 150ms during peak hours.
        """

        claims = self.pipeline.extract_quantitative_claims(uncited_text)
        citation_check = self.pipeline.check_citation_requirements(uncited_text, claims)

        assert citation_check["cited_claims"] < len(claims)
        assert len(citation_check["uncited_claims"]) > 0

    def test_forbidden_terms_detection(self):
        """Test detection of forbidden uncertainty terms."""
        text_with_forbidden_terms = """
        The system probably achieves around 95% success rate.
        Response time seems to be approximately 150ms.
        I think the performance is likely acceptable.
        """

        forbidden_check = self.pipeline.check_forbidden_terms(text_with_forbidden_terms)
        assert forbidden_check["forbidden_terms_found"] > 0

        # Check specific forbidden terms are detected
        found_terms = [v["term"] for v in forbidden_check["violations"]]
        assert "probably" in found_terms
        assert "around" in found_terms
        assert "seems" in found_terms
        assert "approximately" in found_terms

    def test_file_validation_pipeline(self):
        """Test complete file validation pipeline."""
        # Create a test analysis file
        analysis_data = {
            "report_id": "test_001",
            "timestamp": datetime.now(UTC).isoformat(),
            "analysis": """
                Performance Analysis Results

                System success rate: 95.5% [E2: Measured via automated test suite]
                Average response time: 150ms [E1: Verified by monitoring dashboard]
                P95 latency: 300ms [E2: Measured during load test run #847]

                The system demonstrates excellent performance characteristics
                with consistent response times and high reliability.
            """
        }

        # Write test file
        test_file = Path(self.temp_dir) / "test_analysis.json"
        with test_file.open("w") as f:
            json.dump(analysis_data, f)

        # Process through pipeline
        result = self.pipeline.process_analysis_file(test_file)

        assert "validation_steps" in result
        assert "overall_score" in result
        assert result["passed"] is True  # Should pass with proper citations

        # Check that claims were extracted
        claims_found = result["validation_steps"]["claim_extraction"]["claims_found"]
        assert claims_found > 0

# Locate CLI script - check workspace root for multi-repo structure
_workspace_root = Path(__file__).resolve().parents[2]  # Go up from tests/test_*.py -> euxis-metrics -> workspace
_cli_script_path = _workspace_root / "euxis-cli" / "bin" / "euxis-evidence-verify"
_cli_available = _cli_script_path.exists()


@pytest.mark.skipif(not _cli_available, reason="euxis-cli not available in workspace")
class TestCLIInterface(unittest.TestCase):
    """Test the CLI interface functionality."""

    def setUp(self):
        """Set up CLI testing environment."""
        self.cli_script = _cli_script_path

    def test_cli_help(self):
        """Test CLI help functionality."""
        result = subprocess.run(
            [str(self.cli_script), "--help"],
            capture_output=True,
            text=True
        )
        assert result.returncode == 0
        assert "Evidence Verification" in result.stdout

    def test_cli_validate_missing_file(self):
        """Test CLI validation with missing file."""
        result = subprocess.run(
            [str(self.cli_script), "validate", "/nonexistent/file.json"],
            capture_output=True,
            text=True
        )
        assert result.returncode != 0
        assert "ERROR" in result.stdout

def create_sample_analysis_files():
    """Create sample analysis files for manual testing."""
    samples_dir = Path("/tmp/euxis_evidence_samples")
    samples_dir.mkdir(exist_ok=True)

    # Good analysis with proper citations
    good_analysis = {
        "report_id": "sample_good",
        "analysis": """
        Performance Analysis Report - February 2026

        System Metrics:
        - Success rate: 97.2% [E2: Measured via automated monitoring over 24h period]
        - Average response time: 142ms [E1: Verified by production metrics dashboard]
        - P95 latency: 285ms [E2: Measured during peak load test execution]
        - Throughput: 850 requests per second [E1: Verified by load balancer logs]

        Analysis Summary:
        The system demonstrates excellent performance characteristics with
        consistent response times and high reliability metrics.
        """
    }

    # Bad analysis with no citations and forbidden terms
    bad_analysis = {
        "report_id": "sample_bad",
        "analysis": """
        Performance Analysis Report - February 2026

        The system probably achieves around 95% success rate based on our testing.
        Response times seem to be approximately 150ms during normal operation.
        I think the throughput is likely around 800 requests per second.

        Overall, the performance appears acceptable but maybe needs improvement.
        """
    }

    with (samples_dir / "good_analysis.json").open("w") as f:
        json.dump(good_analysis, f, indent=2)

    with (samples_dir / "bad_analysis.json").open("w") as f:
        json.dump(bad_analysis, f, indent=2)

    return samples_dir

if __name__ == "__main__":
    # Create sample files for manual testing
    samples_dir = create_sample_analysis_files()
    _out(f"Created sample files in: {samples_dir}")
    _out("Test with: python3 -m pytest metrics/tests/test_evidence_framework.py -v")

    # Run the tests
    unittest.main()
