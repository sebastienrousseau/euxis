"""Tests for A2A message and artifact types."""

from __future__ import annotations

from euxis_a2a.core.message import A2AMessage, Artifact, ContentPart


def test_content_part_text() -> None:
    part = ContentPart(type="text", content="hello world")
    assert part.type == "text"
    assert part.content == "hello world"
    assert part.mime_type == "text/plain"


def test_content_part_image() -> None:
    part = ContentPart(type="image", content="base64data", mime_type="image/png")
    assert part.type == "image"
    assert part.mime_type == "image/png"


def test_content_part_data() -> None:
    part = ContentPart(type="data", content='{"key": "value"}', mime_type="application/json")
    assert part.type == "data"
    assert part.mime_type == "application/json"


def test_content_part_is_frozen() -> None:
    part = ContentPart(type="text", content="x")
    try:
        part.type = "image"  # type: ignore[misc]
        raise AssertionError("should have raised")
    except AttributeError:
        pass


def test_message_construction_user() -> None:
    part = ContentPart(type="text", content="hello")
    msg = A2AMessage(role="user", parts=(part,))
    assert msg.role == "user"
    assert len(msg.parts) == 1
    assert msg.parts[0].content == "hello"
    assert msg.metadata == {}
    assert msg.timestamp > 0


def test_message_construction_agent() -> None:
    parts = (
        ContentPart(type="text", content="response"),
        ContentPart(type="data", content="extra", mime_type="application/octet-stream"),
    )
    msg = A2AMessage(role="agent", parts=parts, metadata={"model": "test"})
    assert msg.role == "agent"
    assert len(msg.parts) == 2
    assert msg.metadata == {"model": "test"}


def test_message_is_frozen() -> None:
    part = ContentPart(type="text", content="x")
    msg = A2AMessage(role="user", parts=(part,))
    try:
        msg.role = "agent"  # type: ignore[misc]
        raise AssertionError("should have raised")
    except AttributeError:
        pass


def test_artifact_construction() -> None:
    artifact = Artifact(name="output.json", type="json", data='{"result": 42}')
    assert artifact.name == "output.json"
    assert artifact.type == "json"
    assert artifact.data == '{"result": 42}'
    assert artifact.metadata == {}


def test_artifact_with_metadata() -> None:
    artifact = Artifact(
        name="report.pdf", type="pdf", data="base64data",
        metadata={"pages": 10, "author": "agent"},
    )
    assert artifact.metadata["pages"] == 10
    assert artifact.metadata["author"] == "agent"


def test_artifact_is_frozen() -> None:
    artifact = Artifact(name="x", type="y", data="z")
    try:
        artifact.name = "changed"  # type: ignore[misc]
        raise AssertionError("should have raised")
    except AttributeError:
        pass
