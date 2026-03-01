"""Tests for InferenceResult and InferenceUnavailableError."""

from euxis_inference.core.engine import InferenceResult, InferenceUnavailableError


class TestInferenceResult:
    def test_create_result(self) -> None:
        result = InferenceResult(
            text="Hello world",
            tokens_generated=5,
            tokens_per_second=12.5,
            engine_name="test-engine",
            model_name="test-model",
        )
        assert result.text == "Hello world"
        assert result.tokens_generated == 5
        assert result.tokens_per_second == 12.5
        assert result.engine_name == "test-engine"
        assert result.model_name == "test-model"

    def test_result_is_frozen(self) -> None:
        result = InferenceResult(
            text="x", tokens_generated=1, tokens_per_second=1.0,
            engine_name="e", model_name="m",
        )
        try:
            result.text = "changed"  # type: ignore[misc]
            raise AssertionError("Expected FrozenInstanceError")
        except AttributeError:
            pass

    def test_result_equality(self) -> None:
        a = InferenceResult(text="a", tokens_generated=1, tokens_per_second=1.0, engine_name="e", model_name="m")
        b = InferenceResult(text="a", tokens_generated=1, tokens_per_second=1.0, engine_name="e", model_name="m")
        assert a == b


class TestInferenceUnavailableError:
    def test_is_runtime_error(self) -> None:
        err = InferenceUnavailableError("not available")
        assert isinstance(err, RuntimeError)

    def test_message(self) -> None:
        err = InferenceUnavailableError("backend missing")
        assert str(err) == "backend missing"
