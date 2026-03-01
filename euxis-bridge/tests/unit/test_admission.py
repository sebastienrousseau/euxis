"""Tests for multi-stage admission pipeline."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import pytest

from euxis_bridge.core.admission import AdmissionPipeline, AdmissionResult
from euxis_bridge.core.reputation import AuthorReputation, ReputationStore


class TestAdmissionAllPass:
    """Test admission pipeline when all stages pass."""

    def test_clean_skill_admitted(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("def run(): return 42\n")

        pipeline = AdmissionPipeline()
        result = pipeline.evaluate(skill_dir=skill_dir)

        assert result.admitted is True
        assert "static_analysis" in result.stages_passed
        assert "provenance" in result.stages_passed
        assert len(result.stages_failed) == 0

    def test_provenance_record_generated(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("def run(): return 42\n")

        pipeline = AdmissionPipeline()
        result = pipeline.evaluate(skill_dir=skill_dir, source_uri="https://example.com")

        assert result.provenance is not None
        assert result.provenance.source_uri == "https://example.com"
        assert result.provenance.subject_digest.startswith("sha256:")

    def test_empty_dir_admitted(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "empty"
        skill_dir.mkdir()

        pipeline = AdmissionPipeline()
        result = pipeline.evaluate(skill_dir=skill_dir)
        assert result.admitted is True


class TestAdmissionSignatureStage:
    """Test signature verification stage."""

    def test_signature_fail_blocks_admission(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        entrypoint = skill_dir / "main.py"
        entrypoint.write_text("def run(): pass\n")

        pub_key = tmp_path / "key.pub"
        pub_key.write_text("fake-key")

        pipeline = AdmissionPipeline(
            public_key_path=str(pub_key),
            require_signature=True,
        )

        with patch("euxis_bridge.core.admission.verify_skill_signature", return_value=False):
            result = pipeline.evaluate(skill_dir=skill_dir, entrypoint=entrypoint)

        assert result.admitted is False
        assert "signature" in result.stages_failed

    def test_signature_pass_continues(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        entrypoint = skill_dir / "main.py"
        entrypoint.write_text("def run(): pass\n")

        pub_key = tmp_path / "key.pub"
        pub_key.write_text("fake-key")

        pipeline = AdmissionPipeline(
            public_key_path=str(pub_key),
            require_signature=True,
        )

        with patch("euxis_bridge.core.admission.verify_skill_signature", return_value=True):
            result = pipeline.evaluate(skill_dir=skill_dir, entrypoint=entrypoint)

        assert result.admitted is True
        assert "signature" in result.stages_passed

    def test_signature_skipped_when_not_required(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("def run(): pass\n")

        pipeline = AdmissionPipeline(require_signature=False)
        result = pipeline.evaluate(skill_dir=skill_dir)

        assert result.admitted is True
        assert "signature" not in result.stages_passed
        assert "signature" not in result.stages_failed


class TestAdmissionStaticAnalysisStage:
    """Test static analysis stage."""

    def test_malicious_pattern_blocks_admission(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "evil.py").write_text("exec('rm -rf /')\n")

        pipeline = AdmissionPipeline()
        result = pipeline.evaluate(skill_dir=skill_dir)

        assert result.admitted is False
        assert "static_analysis" in result.stages_failed
        assert len(result.findings) > 0

    def test_js_eval_blocks_admission(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "bad.js").write_text("eval('hack()');\n")

        pipeline = AdmissionPipeline()
        result = pipeline.evaluate(skill_dir=skill_dir)

        assert result.admitted is False
        assert "static_analysis" in result.stages_failed

    def test_warning_only_passes(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "env.js").write_text("const k = process.env.KEY;\n")

        pipeline = AdmissionPipeline()
        result = pipeline.evaluate(skill_dir=skill_dir)

        assert result.admitted is True
        assert "static_analysis" in result.stages_passed

    def test_analysis_failure_prevents_provenance(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "evil.py").write_text("exec('bad')\n")

        pipeline = AdmissionPipeline()
        result = pipeline.evaluate(skill_dir=skill_dir)

        assert result.admitted is False
        assert result.provenance is None  # Never reached provenance stage


class TestAdmissionReputationStage:
    """Test reputation check stage."""

    def test_low_reputation_blocks_admission(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("def run(): pass\n")

        store = ReputationStore(tmp_path / "rep.json")
        store.update_reputation(AuthorReputation(author_id="bad-actor", score=0.1))

        pipeline = AdmissionPipeline(
            reputation_store=store,
            require_reputation=True,
            reputation_threshold=0.3,
        )
        result = pipeline.evaluate(skill_dir=skill_dir, author_id="bad-actor")

        assert result.admitted is False
        assert "reputation" in result.stages_failed
        assert result.reputation is not None
        assert result.reputation.score == 0.1

    def test_good_reputation_passes(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("def run(): pass\n")

        store = ReputationStore(tmp_path / "rep.json")
        store.update_reputation(AuthorReputation(author_id="trusted", score=0.9))

        pipeline = AdmissionPipeline(
            reputation_store=store,
            require_reputation=True,
            reputation_threshold=0.3,
        )
        result = pipeline.evaluate(skill_dir=skill_dir, author_id="trusted")

        assert result.admitted is True
        assert "reputation" in result.stages_passed

    def test_reputation_skipped_when_not_required(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("def run(): pass\n")

        pipeline = AdmissionPipeline(require_reputation=False)
        result = pipeline.evaluate(skill_dir=skill_dir)

        assert result.admitted is True
        assert "reputation" not in result.stages_passed
        assert "reputation" not in result.stages_failed
        assert result.reputation is None

    def test_reputation_preserves_provenance_on_fail(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("def run(): pass\n")

        store = ReputationStore(tmp_path / "rep.json")
        store.update_reputation(AuthorReputation(author_id="low", score=0.05))

        pipeline = AdmissionPipeline(
            reputation_store=store,
            require_reputation=True,
            reputation_threshold=0.3,
        )
        result = pipeline.evaluate(skill_dir=skill_dir, author_id="low")

        assert result.admitted is False
        # Provenance was generated before reputation was checked
        assert result.provenance is not None


class TestAdmissionResultStructure:
    """Test AdmissionResult structure and invariants."""

    def test_result_is_frozen(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("pass\n")

        pipeline = AdmissionPipeline()
        result = pipeline.evaluate(skill_dir=skill_dir)

        with pytest.raises(AttributeError):
            result.admitted = False  # type: ignore[misc]

    def test_all_stages_in_order(self, tmp_path: Path) -> None:
        skill_dir = tmp_path / "skill"
        skill_dir.mkdir()
        (skill_dir / "main.py").write_text("def run(): pass\n")

        store = ReputationStore(tmp_path / "rep.json")
        store.update_reputation(AuthorReputation(author_id="author", score=0.8))

        pipeline = AdmissionPipeline(
            reputation_store=store,
            require_reputation=True,
        )

        with patch("euxis_bridge.core.admission.verify_skill_signature", return_value=True):
            result = pipeline.evaluate(
                skill_dir=skill_dir,
                entrypoint=skill_dir / "main.py",
                author_id="author",
            )

        # Signature not required, so not in passed
        assert result.admitted is True
        assert "static_analysis" in result.stages_passed
        assert "provenance" in result.stages_passed
        assert "reputation" in result.stages_passed
