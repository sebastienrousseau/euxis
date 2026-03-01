"""Tests for QualityGate assessment."""

from euxis_inference.core.quality_gate import QualityGate, QualityScore


class TestQualityGate:
    def test_good_response_passes(self) -> None:
        gate = QualityGate(threshold=0.5)
        score = gate.assess("What is Python?", "Python is a programming language used widely.")
        assert score.passed is True
        assert score.overall >= 0.5

    def test_empty_response_fails(self) -> None:
        gate = QualityGate(threshold=0.5)
        score = gate.assess("Tell me about Rust", "")
        assert score.passed is False
        assert score.coherence == 0.0
        assert score.relevance == 0.0
        assert score.overall == 0.0

    def test_whitespace_only_fails(self) -> None:
        gate = QualityGate(threshold=0.5)
        score = gate.assess("Hello", "   \n\t  ")
        assert score.passed is False

    def test_short_response_penalized(self) -> None:
        gate = QualityGate(threshold=0.5)
        score = gate.assess("Explain quantum physics", "Yes")
        assert score.coherence < 1.0

    def test_repetitive_response_penalized(self) -> None:
        gate = QualityGate(threshold=0.5)
        repeated = "word " * 20
        score = gate.assess("Explain something", repeated)
        assert score.coherence < 1.0

    def test_high_threshold_strict(self) -> None:
        gate = QualityGate(threshold=0.95)
        score = gate.assess("What is X?", "Short answer")
        assert score.passed is False

    def test_low_threshold_lenient(self) -> None:
        gate = QualityGate(threshold=0.1)
        score = gate.assess("Hello", "Hello there, how are you doing today?")
        assert score.passed is True

    def test_no_alpha_chars_penalized(self) -> None:
        gate = QualityGate(threshold=0.5)
        score = gate.assess("What is 2+2?", "12345 67890")
        assert score.coherence < 1.0

    def test_score_fields_bounded(self) -> None:
        gate = QualityGate(threshold=0.5)
        score = gate.assess("test prompt", "test response with enough words here for checking")
        assert 0.0 <= score.coherence <= 1.0
        assert 0.0 <= score.relevance <= 1.0
        assert 0.0 <= score.overall <= 1.0

    def test_relevance_boosted_by_overlap(self) -> None:
        gate = QualityGate(threshold=0.5)
        score = gate.assess("Python programming language", "Python is a great programming language")
        assert score.relevance > 0.5

    def test_quality_score_is_frozen(self) -> None:
        score = QualityScore(coherence=0.8, relevance=0.7, overall=0.75, passed=True)
        try:
            score.coherence = 0.0  # type: ignore[misc]
            raise AssertionError("Expected FrozenInstanceError")
        except AttributeError:
            pass

    def test_empty_prompt_relevance(self) -> None:
        gate = QualityGate(threshold=0.5)
        score = gate.assess("", "Some response text here")
        assert score.relevance == 0.5
