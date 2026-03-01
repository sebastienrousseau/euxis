"""Tests for A2A server handler."""

from __future__ import annotations

from euxis_a2a.core.agent_card import AgentCard, AuthSchema, Capability
from euxis_a2a.core.server import A2AServerHandler
from euxis_a2a.core.task import TaskStatus


def _make_card(caps: tuple[Capability, ...] = ()) -> AgentCard:
    return AgentCard(
        name="test-server",
        description="Test server agent",
        url="https://test.example.com",
        capabilities=caps,
        auth=AuthSchema(type="none"),
    )


def test_handle_agent_card() -> None:
    card = _make_card()
    handler = A2AServerHandler(card)
    result = handler.handle_request("agent_card")
    assert result["name"] == "test-server"
    assert result["url"] == "https://test.example.com"


def test_handle_task_create() -> None:
    handler = A2AServerHandler(_make_card())
    result = handler.handle_request("task_create", {"message": "do something"})
    assert "task_id" in result
    assert result["status"] == TaskStatus.PENDING


def test_handle_task_create_no_message() -> None:
    handler = A2AServerHandler(_make_card())
    result = handler.handle_request("task_create")
    assert "task_id" in result
    assert result["status"] == TaskStatus.PENDING


def test_handle_task_get_existing() -> None:
    handler = A2AServerHandler(_make_card())
    created = handler.handle_request("task_create", {"message": "test"})
    task_id = created["task_id"]
    result = handler.handle_request("task_get", {"task_id": task_id})
    assert result["task_id"] == task_id
    assert result["status"] == TaskStatus.PENDING
    assert result["messages"] == 1


def test_handle_task_get_not_found() -> None:
    handler = A2AServerHandler(_make_card())
    result = handler.handle_request("task_get", {"task_id": "nonexistent"})
    assert result["error"] == "task_not_found"
    assert result["task_id"] == "nonexistent"


def test_handle_task_get_no_task_id() -> None:
    handler = A2AServerHandler(_make_card())
    result = handler.handle_request("task_get")
    assert result["error"] == "task_not_found"


def test_handle_task_cancel_existing() -> None:
    handler = A2AServerHandler(_make_card())
    created = handler.handle_request("task_create")
    task_id = created["task_id"]
    result = handler.handle_request("task_cancel", {"task_id": task_id})
    assert result["task_id"] == task_id
    assert result["status"] == TaskStatus.CANCELLED


def test_handle_task_cancel_not_found() -> None:
    handler = A2AServerHandler(_make_card())
    result = handler.handle_request("task_cancel", {"task_id": "missing"})
    assert result["error"] == "task_not_found"


def test_handle_task_cancel_invalid_transition() -> None:
    handler = A2AServerHandler(_make_card())
    created = handler.handle_request("task_create")
    task_id = created["task_id"]
    # Cancel once (valid)
    handler.handle_request("task_cancel", {"task_id": task_id})
    # Cancel again (invalid -- already cancelled)
    result = handler.handle_request("task_cancel", {"task_id": task_id})
    assert "error" in result
    assert "invalid transition" in result["error"]


def test_handle_capabilities_list() -> None:
    caps = (
        Capability(name="summarize", description="Summarize text"),
        Capability(name="translate", description="Translate text"),
    )
    handler = A2AServerHandler(_make_card(caps))
    result = handler.handle_request("capabilities_list")
    assert result["capabilities"] == ["summarize", "translate"]


def test_handle_capabilities_list_empty() -> None:
    handler = A2AServerHandler(_make_card())
    result = handler.handle_request("capabilities_list")
    assert result["capabilities"] == []


def test_handle_unknown_method() -> None:
    handler = A2AServerHandler(_make_card())
    result = handler.handle_request("unknown_method_xyz")
    assert result["error"] == "unknown_method"
    assert result["method"] == "unknown_method_xyz"
