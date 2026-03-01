"""Quality gate for inference output assessment."""

from __future__ import annotations

import re
from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class QualityScore:
    coherence: float  # 0.0 to 1.0
    relevance: float  # 0.0 to 1.0
    overall: float  # 0.0 to 1.0
    passed: bool


class QualityGate:
    """Heuristic quality assessment for inference outputs."""

    def __init__(self, threshold: float = 0.5) -> None:
        self.threshold = threshold

    def assess(self, prompt: str, response: str) -> QualityScore:
        coherence = self._check_coherence(response)
        relevance = self._check_relevance(prompt, response)
        overall = (coherence + relevance) / 2.0
        return QualityScore(
            coherence=round(coherence, 3),
            relevance=round(relevance, 3),
            overall=round(overall, 3),
            passed=overall >= self.threshold,
        )

    def _check_coherence(self, response: str) -> float:
        if not response.strip():
            return 0.0
        score = 1.0
        # Penalize very short responses
        if len(response.strip()) < 10:
            score -= 0.3
        # Penalize excessive repetition
        words = response.lower().split()
        if len(words) > 5:
            unique_ratio = len(set(words)) / len(words)
            if unique_ratio < 0.3:
                score -= 0.5
        # Penalize pure whitespace/garbage
        if not re.search(r"[a-zA-Z]", response):
            score -= 0.5
        return max(0.0, min(1.0, score))

    def _check_relevance(self, prompt: str, response: str) -> float:
        if not response.strip():
            return 0.0
        prompt_words = set(prompt.lower().split())
        response_words = set(response.lower().split())
        if not prompt_words:
            return 0.5
        overlap = prompt_words & response_words
        ratio = len(overlap) / max(len(prompt_words), 1)
        return max(0.0, min(1.0, ratio + 0.3))
