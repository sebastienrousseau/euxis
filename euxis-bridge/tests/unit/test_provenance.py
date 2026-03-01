"""Tests for SLSA v1.0 compatible provenance tracking."""

from __future__ import annotations

from pathlib import Path

import pytest

from euxis_bridge.core.provenance import ProvenanceChain, ProvenanceRecord


class TestProvenanceChainGenerate:
    """Test provenance record generation."""

    def test_generate_basic_record(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("print('hello')\n")
        chain = ProvenanceChain(builder_id="test-builder")
        record = chain.generate("my-skill", tmp_path, source_uri="https://example.com")

        assert record.subject_name == "my-skill"
        assert record.subject_digest.startswith("sha256:")
        assert record.builder_id == "test-builder"
        assert record.source_uri == "https://example.com"
        assert len(record.timestamp) > 0

    def test_generate_collects_materials(self, tmp_path: Path) -> None:
        (tmp_path / "a.py").write_text("a\n")
        (tmp_path / "b.js").write_text("b\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)

        assert len(record.materials) == 2
        uris = {m["uri"] for m in record.materials}
        assert "a.py" in uris
        assert "b.js" in uris
        assert all(m["digest"].startswith("sha256:") for m in record.materials)

    def test_generate_deterministic_digest(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("content\n")
        chain = ProvenanceChain()
        r1 = chain.generate("skill", tmp_path)
        r2 = chain.generate("skill", tmp_path)
        assert r1.subject_digest == r2.subject_digest

    def test_generate_empty_directory(self, tmp_path: Path) -> None:
        chain = ProvenanceChain()
        record = chain.generate("empty", tmp_path)
        assert record.subject_digest.startswith("sha256:")
        assert len(record.materials) == 0

    def test_nested_files_included(self, tmp_path: Path) -> None:
        sub = tmp_path / "lib"
        sub.mkdir()
        (sub / "util.py").write_text("util\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)
        uris = {m["uri"] for m in record.materials}
        assert any("util.py" in u for u in uris)


class TestProvenanceChainVerify:
    """Test provenance verification."""

    def test_verify_matching_directory(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("content\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)
        assert chain.verify(record, tmp_path) is True

    def test_verify_tampered_directory(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("content\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)

        # Tamper with the file
        (tmp_path / "main.py").write_text("tampered\n")
        assert chain.verify(record, tmp_path) is False

    def test_verify_added_file(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("content\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)

        # Add a new file
        (tmp_path / "extra.py").write_text("extra\n")
        assert chain.verify(record, tmp_path) is False

    def test_verify_deleted_file(self, tmp_path: Path) -> None:
        (tmp_path / "a.py").write_text("a\n")
        (tmp_path / "b.py").write_text("b\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)

        (tmp_path / "b.py").unlink()
        assert chain.verify(record, tmp_path) is False


class TestProvenanceChainExportBundle:
    """Test SLSA v1.0 bundle export."""

    def test_export_has_intoto_type(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("x\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)
        bundle = chain.export_bundle(record)

        assert bundle["_type"] == "https://in-toto.io/Statement/v1"

    def test_export_has_slsa_predicate_type(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("x\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)
        bundle = chain.export_bundle(record)

        assert bundle["predicateType"] == "https://slsa.dev/provenance/v1"

    def test_export_subject_structure(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("x\n")
        chain = ProvenanceChain()
        record = chain.generate("my-skill", tmp_path)
        bundle = chain.export_bundle(record)

        subjects = bundle["subject"]
        assert len(subjects) == 1
        assert subjects[0]["name"] == "my-skill"
        assert "sha256" in subjects[0]["digest"]
        # Digest value should NOT have the sha256: prefix
        assert not subjects[0]["digest"]["sha256"].startswith("sha256:")

    def test_export_builder_id(self, tmp_path: Path) -> None:
        (tmp_path / "main.py").write_text("x\n")
        chain = ProvenanceChain(builder_id="custom-builder")
        record = chain.generate("skill", tmp_path)
        bundle = chain.export_bundle(record)

        builder = bundle["predicate"]["runDetails"]["builder"]
        assert builder["id"] == "custom-builder"

    def test_export_materials_list(self, tmp_path: Path) -> None:
        (tmp_path / "a.py").write_text("a\n")
        chain = ProvenanceChain()
        record = chain.generate("skill", tmp_path)
        bundle = chain.export_bundle(record)

        assert isinstance(bundle["materials"], list)
        assert len(bundle["materials"]) == 1
