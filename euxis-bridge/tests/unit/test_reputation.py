"""Tests for author reputation store."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from euxis_bridge.core.reputation import AuthorReputation, ReputationStore


class TestAuthorReputation:
    """Test AuthorReputation dataclass."""

    def test_defaults(self) -> None:
        rep = AuthorReputation(author_id="alice", score=0.8)
        assert rep.author_id == "alice"
        assert rep.score == 0.8
        assert rep.attestations == 0
        assert rep.history_count == 0
        assert rep.flags == ()

    def test_with_flags(self) -> None:
        rep = AuthorReputation(
            author_id="bob", score=0.2, flags=("malware_author",)
        )
        assert rep.flags == ("malware_author",)

    def test_frozen(self) -> None:
        rep = AuthorReputation(author_id="alice", score=0.5)
        with pytest.raises(AttributeError):
            rep.score = 0.9  # type: ignore[misc]


class TestReputationStore:
    """Test file-backed reputation store."""

    def test_unknown_author_gets_default(self, tmp_path: Path) -> None:
        store = ReputationStore(tmp_path / "rep.json")
        rep = store.get_reputation("unknown-user")
        assert rep.author_id == "unknown-user"
        assert rep.score == 0.5

    def test_update_and_retrieve(self, tmp_path: Path) -> None:
        store = ReputationStore(tmp_path / "rep.json")
        rep = AuthorReputation(
            author_id="alice", score=0.9, attestations=5, history_count=10
        )
        store.update_reputation(rep)

        retrieved = store.get_reputation("alice")
        assert retrieved.score == 0.9
        assert retrieved.attestations == 5
        assert retrieved.history_count == 10

    def test_check_threshold_passes(self, tmp_path: Path) -> None:
        store = ReputationStore(tmp_path / "rep.json")
        store.update_reputation(AuthorReputation(author_id="alice", score=0.8))
        assert store.check_threshold("alice", threshold=0.3) is True

    def test_check_threshold_fails(self, tmp_path: Path) -> None:
        store = ReputationStore(tmp_path / "rep.json")
        store.update_reputation(AuthorReputation(author_id="bad", score=0.1))
        assert store.check_threshold("bad", threshold=0.3) is False

    def test_check_threshold_exact_boundary(self, tmp_path: Path) -> None:
        store = ReputationStore(tmp_path / "rep.json")
        store.update_reputation(AuthorReputation(author_id="edge", score=0.3))
        assert store.check_threshold("edge", threshold=0.3) is True

    def test_unknown_author_default_passes_threshold(self, tmp_path: Path) -> None:
        store = ReputationStore(tmp_path / "rep.json")
        # Default score is 0.5, threshold 0.3 should pass
        assert store.check_threshold("nobody", threshold=0.3) is True

    def test_persistence_across_instances(self, tmp_path: Path) -> None:
        store_path = tmp_path / "rep.json"
        store1 = ReputationStore(store_path)
        store1.update_reputation(
            AuthorReputation(author_id="alice", score=0.7, attestations=3)
        )

        store2 = ReputationStore(store_path)
        rep = store2.get_reputation("alice")
        assert rep.score == 0.7
        assert rep.attestations == 3

    def test_persistence_file_is_valid_json(self, tmp_path: Path) -> None:
        store_path = tmp_path / "rep.json"
        store = ReputationStore(store_path)
        store.update_reputation(AuthorReputation(author_id="x", score=0.5))
        data = json.loads(store_path.read_text(encoding="utf-8"))
        assert "x" in data
        assert data["x"]["score"] == 0.5

    def test_flags_round_trip(self, tmp_path: Path) -> None:
        store_path = tmp_path / "rep.json"
        store = ReputationStore(store_path)
        store.update_reputation(
            AuthorReputation(author_id="flagged", score=0.1, flags=("spam", "abuse"))
        )

        store2 = ReputationStore(store_path)
        rep = store2.get_reputation("flagged")
        assert rep.flags == ("spam", "abuse")

    def test_creates_parent_dirs(self, tmp_path: Path) -> None:
        store_path = tmp_path / "nested" / "dir" / "rep.json"
        store = ReputationStore(store_path)
        store.update_reputation(AuthorReputation(author_id="alice", score=0.9))
        assert store_path.exists()
