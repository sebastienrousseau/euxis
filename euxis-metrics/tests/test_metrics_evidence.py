# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Extended unit tests for euxis-metrics modules with low coverage.

Covers:
- metrics/__init__.py: get_performance_collector, get_fast_collector
- metrics/collectors/performance_collector.py: PerformanceMetricsCollector
- metrics/collectors/fast_collector.py: FastMetricsCollector, FastMetricsBuffer
- metrics/aggregators/performance_analyzer.py: PerformanceAnalyzer
- metrics/verification/validation_pipeline.py: ValidationPipeline
- metrics/verification/evidence_framework.py: EvidenceFramework
"""

from __future__ import annotations

import asyncio
import json
import os
import time
from datetime import UTC, datetime, timedelta
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

# ============================================================================
# metrics/__init__.py
# ============================================================================


class TestEvidenceFramework:
    """Tests for EvidenceFramework."""

    def test_init_creates_directories(self, tmp_path):
        """__init__ should create evidence directory."""
        from metrics.verification.evidence_framework import EvidenceFramework
        ev_dir = tmp_path / "evidence"
        framework = EvidenceFramework(str(ev_dir))
        assert ev_dir.exists()

    def test_store_and_load_evidence(self, tmp_path):
        """store_evidence and load_evidence should round-trip."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=42,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="95.5 percent", timestamp=datetime.now(UTC),
            verification_cmd="echo ok", metadata={"key": "val"},
        )

        ev_hash = framework.store_evidence(evidence)
        loaded = framework.load_evidence(ev_hash)
        assert loaded is not None
        assert loaded.source_file == "test.py"
        assert loaded.source_line == 42
        assert loaded.grade == EvidenceGrade.MEASURED

    def test_load_evidence_not_found(self, tmp_path):
        """load_evidence for nonexistent hash should return None."""
        from metrics.verification.evidence_framework import EvidenceFramework
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        assert framework.load_evidence("nonexistent") is None

    def test_load_evidence_no_db(self, tmp_path):
        """load_evidence when DB file doesn't exist should return None."""
        from metrics.verification.evidence_framework import EvidenceFramework
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        # Don't write anything; DB doesn't exist
        assert framework.load_evidence("any_hash") is None

    def test_load_evidence_malformed_lines(self, tmp_path):
        """load_evidence should skip malformed JSON lines."""
        from metrics.verification.evidence_framework import EvidenceFramework
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        with framework.evidence_db.open("w") as f:
            f.write("not json\n")
            f.write("{}\n")  # valid JSON but missing keys
        assert framework.load_evidence("any") is None

    def test_verify_evidence_no_cmd(self, tmp_path):
        """verify_evidence without verification_cmd should return False."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="obs", grade=EvidenceGrade.OBSERVED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd=None, metadata={},
        )
        assert framework.verify_evidence(evidence) is False

    def test_verify_evidence_success(self, tmp_path):
        """verify_evidence with successful command should return True."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.VERIFIED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd="echo ok", metadata={},
        )

        with patch("metrics.verification.evidence_framework.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            assert framework.verify_evidence(evidence) is True

    def test_verify_evidence_failure(self, tmp_path):
        """verify_evidence with failed command should return False."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.VERIFIED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd="false", metadata={},
        )

        with patch("metrics.verification.evidence_framework.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=1)
            assert framework.verify_evidence(evidence) is False

    def test_verify_evidence_shell_command(self, tmp_path):
        """verify_evidence with shell metacharacters should use shell=True."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.VERIFIED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd="echo ok && echo done", metadata={},
        )

        with patch("metrics.verification.evidence_framework.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            result = framework.verify_evidence(evidence)
            # Should have been called with shell=True due to &&
            mock_run.assert_called_once()
            call_kwargs = mock_run.call_args
            assert call_kwargs.kwargs.get("shell") is True

    def test_verify_evidence_timeout(self, tmp_path):
        """verify_evidence with timeout should return False."""
        import subprocess
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.VERIFIED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd="sleep 100", metadata={},
        )

        with patch("metrics.verification.evidence_framework.subprocess.run") as mock_run:
            mock_run.side_effect = subprocess.TimeoutExpired("sleep", 30)
            assert framework.verify_evidence(evidence) is False

    def test_verify_evidence_os_error(self, tmp_path):
        """verify_evidence with OSError should return False."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.VERIFIED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd="nonexistent", metadata={},
        )

        with patch("metrics.verification.evidence_framework.subprocess.run") as mock_run:
            mock_run.side_effect = OSError("cmd not found")
            assert framework.verify_evidence(evidence) is False

    def test_check_evidence_decay_fresh(self, tmp_path):
        """Fresh evidence should not be decayed."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd=None, metadata={},
        )
        assert framework.check_evidence_decay(evidence) is False

    def test_check_evidence_decay_old(self, tmp_path):
        """Old evidence should be marked as decayed."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="data",
            timestamp=datetime.now(UTC) - timedelta(days=5),
            verification_cmd=None, metadata={},
        )
        assert framework.check_evidence_decay(evidence) is True

    def test_validate_claim_no_evidence(self, tmp_path):
        """Claim with no evidence should be invalid."""
        from metrics.verification.evidence_framework import Claim, EvidenceFramework
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        claim = Claim(
            statement="Rate is 95%", value=95.0, unit="percent",
            context="test", supporting_evidence=[], confidence=0.9,
        )
        result = framework.validate_claim(claim)
        assert result["valid"] is False
        assert any("No supporting" in i for i in result["issues"])

    def test_validate_claim_e5_forbidden(self, tmp_path):
        """Claim with E5 evidence should be invalid."""
        from metrics.verification.evidence_framework import (
            Claim,
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="guess", grade=EvidenceGrade.SPECULATED,
            content="maybe 95%", timestamp=datetime.now(UTC),
            verification_cmd=None, metadata={},
        )
        claim = Claim(
            statement="Rate is 95%", value=95.0, unit="percent",
            context="test", supporting_evidence=[evidence], confidence=0.9,
        )
        result = framework.validate_claim(claim)
        assert result["valid"] is False
        assert any("E5" in i for i in result["issues"])

    def test_validate_claim_e4_only_insufficient(self, tmp_path):
        """Claim with only E4 evidence should fail (requires at least E3)."""
        from metrics.verification.evidence_framework import (
            Claim,
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="inference", grade=EvidenceGrade.INFERRED,
            content="inferred 95%", timestamp=datetime.now(UTC),
            verification_cmd=None, metadata={},
        )
        claim = Claim(
            statement="Rate is 95%", value=95.0, unit="percent",
            context="test", supporting_evidence=[evidence], confidence=0.9,
        )
        result = framework.validate_claim(claim)
        assert result["valid"] is False

    def test_validate_claim_valid_e1(self, tmp_path):
        """Claim with E1 evidence and successful verification should be valid."""
        from metrics.verification.evidence_framework import (
            Claim,
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="test_result", grade=EvidenceGrade.VERIFIED,
            content="95%", timestamp=datetime.now(UTC),
            verification_cmd="echo ok", metadata={},
        )
        claim = Claim(
            statement="Rate is 95%", value=95.0, unit="percent",
            context="test", supporting_evidence=[evidence], confidence=0.9,
        )

        with patch("metrics.verification.evidence_framework.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            result = framework.validate_claim(claim)

        assert result["valid"] is True
        assert result["evidence_summary"]["verified_evidence"] == 1

    def test_validate_analysis_report(self, tmp_path):
        """validate_analysis_report should validate all claims in the report."""
        from metrics.verification.evidence_framework import (
            Claim,
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="95%", timestamp=datetime.now(UTC),
            verification_cmd=None, metadata={},
        )
        claim = Claim(
            statement="Rate is 95%", value=95.0, unit="percent",
            context="test", supporting_evidence=[evidence], confidence=0.9,
        )
        report = {"report_id": "test-001"}
        result = framework.validate_analysis_report(report, [claim])
        assert result["claims_total"] == 1
        assert result["report_id"] == "test-001"

    def test_validate_analysis_report_insufficient_evidence(self, tmp_path):
        """Report with fewer evidence than claims should note it."""
        from metrics.verification.evidence_framework import (
            Claim,
            EvidenceFramework,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        claim1 = Claim(
            statement="A", value=1.0, unit="x",
            context="test", supporting_evidence=[], confidence=0.5,
        )
        claim2 = Claim(
            statement="B", value=2.0, unit="x",
            context="test", supporting_evidence=[], confidence=0.5,
        )
        result = framework.validate_analysis_report({}, [claim1, claim2])
        assert any("Insufficient" in i for i in result["issues"])

    def test_generate_citation_format_with_line(self, tmp_path):
        """generate_citation_format should include source line when present."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=42,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd=None, metadata={},
        )
        citation = framework.generate_citation_format(evidence)
        assert citation == "[E2: measurement in test.py:42]"

    def test_generate_citation_format_without_line(self, tmp_path):
        """generate_citation_format should omit line number when None."""
        from metrics.verification.evidence_framework import (
            Evidence,
            EvidenceFramework,
            EvidenceGrade,
        )
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = Evidence(
            source_file="test.py", source_line=None,
            evidence_type="observation", grade=EvidenceGrade.OBSERVED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd=None, metadata={},
        )
        citation = framework.generate_citation_format(evidence)
        assert citation == "[E3: observation in test.py]"

    def test_extract_claims_from_text(self, tmp_path):
        """extract_claims_from_text should find quantitative claims."""
        from metrics.verification.evidence_framework import EvidenceFramework
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        claims = framework.extract_claims_from_text(
            "The success rate of 95.5% was achieved. "
            "Response time was 250 ms. P95 42.0 was measured."
        )
        assert len(claims) >= 2

    def test_create_evidence_from_measurement(self, tmp_path):
        """create_evidence_from_measurement should return E2 evidence."""
        from metrics.verification.evidence_framework import EvidenceFramework, EvidenceGrade
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = framework.create_evidence_from_measurement(
            "perf.jsonl", 95.5, "percent", "echo ok",
        )
        assert evidence.grade == EvidenceGrade.MEASURED
        assert evidence.evidence_type == "measurement"

    def test_create_evidence_from_observation(self, tmp_path):
        """create_evidence_from_observation should return E3 evidence."""
        from metrics.verification.evidence_framework import EvidenceFramework, EvidenceGrade
        framework = EvidenceFramework(str(tmp_path / "evidence"))
        evidence = framework.create_evidence_from_observation(
            "code.py", 42, "Found rate limit logic",
        )
        assert evidence.grade == EvidenceGrade.OBSERVED
        assert evidence.source_line == 42

    def test_claim_get_highest_evidence_grade_empty(self):
        """Claim with no evidence should return None for highest grade."""
        from metrics.verification.evidence_framework import Claim
        claim = Claim(
            statement="x", value=1, unit="x",
            context="c", supporting_evidence=[], confidence=0.5,
        )
        assert claim.get_highest_evidence_grade() is None

    def test_claim_get_highest_evidence_grade(self, tmp_path):
        """Claim should return highest (lowest index) grade."""
        from metrics.verification.evidence_framework import (
            Claim,
            Evidence,
            EvidenceGrade,
        )
        e1 = Evidence("f", 1, "t", EvidenceGrade.OBSERVED, "c",
                       datetime.now(UTC), None, {})
        e2 = Evidence("f", 2, "t", EvidenceGrade.VERIFIED, "c",
                       datetime.now(UTC), None, {})
        claim = Claim("s", 1, "u", "ctx", [e1, e2], 0.9)
        assert claim.get_highest_evidence_grade() == EvidenceGrade.VERIFIED

    def test_evidence_get_hash(self):
        """Evidence.get_hash should return consistent hash."""
        from metrics.verification.evidence_framework import Evidence, EvidenceGrade
        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="data", timestamp=datetime(2026, 1, 1, tzinfo=UTC),
            verification_cmd=None, metadata={},
        )
        h1 = evidence.get_hash()
        h2 = evidence.get_hash()
        assert h1 == h2
        assert len(h1) == 12


class TestCreatePerformanceClaimWithEvidence:
    """Tests for module-level create_performance_claim_with_evidence."""

    def test_creates_claim_with_e2_evidence(self):
        """Should create a claim backed by E2 evidence."""
        from metrics.verification.evidence_framework import (
            EvidenceGrade,
            create_performance_claim_with_evidence,
        )
        claim = create_performance_claim_with_evidence(
            "Fleet success rate is 95.5%",
            95.5, "percent", "perf.jsonl", "echo ok",
        )
        assert claim.statement == "Fleet success rate is 95.5%"
        assert claim.value == 95.5
        assert len(claim.supporting_evidence) == 1
        assert claim.supporting_evidence[0].grade == EvidenceGrade.MEASURED
