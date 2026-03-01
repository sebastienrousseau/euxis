"""Author reputation store for skill trust scoring."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path


@dataclass(frozen=True, slots=True)
class AuthorReputation:
    author_id: str
    score: float  # 0.0 to 1.0
    attestations: int = 0
    history_count: int = 0
    flags: tuple[str, ...] = ()


class ReputationStore:
    """File-backed JSON reputation store."""

    def __init__(self, store_path: Path) -> None:
        self._path = store_path
        self._data: dict[str, dict] = {}
        if self._path.exists():
            self._data = json.loads(self._path.read_text(encoding="utf-8"))

    def _save(self) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._path.write_text(json.dumps(self._data, indent=2), encoding="utf-8")

    def get_reputation(self, author_id: str) -> AuthorReputation:
        if author_id not in self._data:
            return AuthorReputation(author_id=author_id, score=0.5)
        entry = self._data[author_id]
        return AuthorReputation(
            author_id=author_id,
            score=entry.get("score", 0.5),
            attestations=entry.get("attestations", 0),
            history_count=entry.get("history_count", 0),
            flags=tuple(entry.get("flags", [])),
        )

    def update_reputation(self, reputation: AuthorReputation) -> None:
        self._data[reputation.author_id] = {
            "score": reputation.score,
            "attestations": reputation.attestations,
            "history_count": reputation.history_count,
            "flags": list(reputation.flags),
        }
        self._save()

    def check_threshold(self, author_id: str, threshold: float = 0.3) -> bool:
        return self.get_reputation(author_id).score >= threshold
