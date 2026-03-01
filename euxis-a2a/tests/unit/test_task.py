"""Tests for A2A task types and state management."""

from __future__ import annotations

import pytest

from euxis_a2a.core.task import (
    A2ATask,
    TaskStatus,
    create_task,
    transition_task,
)


def test_task_status_constants() -> None:
    assert TaskStatus.PENDING == "pending"
    assert TaskStatus.ACTIVE == "active"
    assert TaskStatus.COMPLETED == "completed"
    assert TaskStatus.FAILED == "failed"
    assert TaskStatus.CANCELLED == "cancelled"


def test_create_task_no_message() -> None:
    task = create_task()
    assert task.status == TaskStatus.PENDING
    assert len(task.id) == 32  # uuid4 hex
    assert task.messages == []
    assert task.artifacts == []
    assert task.history == []
    assert task.created_at > 0
    assert task.updated_at > 0


def test_create_task_with_message() -> None:
    task = create_task(message="hello")
    assert task.messages == ["hello"]


def test_transition_pending_to_active() -> None:
    task = create_task()
    result = transition_task(task, TaskStatus.ACTIVE)
    assert result is task
    assert task.status == TaskStatus.ACTIVE
    assert len(task.history) == 1
    assert task.history[0] == {"from": "pending", "to": "active"}


def test_transition_pending_to_cancelled() -> None:
    task = create_task()
    transition_task(task, TaskStatus.CANCELLED)
    assert task.status == TaskStatus.CANCELLED


def test_transition_active_to_completed() -> None:
    task = create_task()
    transition_task(task, TaskStatus.ACTIVE)
    transition_task(task, TaskStatus.COMPLETED)
    assert task.status == TaskStatus.COMPLETED
    assert len(task.history) == 2


def test_transition_active_to_failed() -> None:
    task = create_task()
    transition_task(task, TaskStatus.ACTIVE)
    transition_task(task, TaskStatus.FAILED)
    assert task.status == TaskStatus.FAILED


def test_transition_active_to_cancelled() -> None:
    task = create_task()
    transition_task(task, TaskStatus.ACTIVE)
    transition_task(task, TaskStatus.CANCELLED)
    assert task.status == TaskStatus.CANCELLED


def test_invalid_transition_pending_to_completed() -> None:
    task = create_task()
    with pytest.raises(ValueError, match="invalid transition: pending -> completed"):
        transition_task(task, TaskStatus.COMPLETED)


def test_invalid_transition_completed_to_active() -> None:
    task = create_task()
    transition_task(task, TaskStatus.ACTIVE)
    transition_task(task, TaskStatus.COMPLETED)
    with pytest.raises(ValueError, match="invalid transition: completed -> active"):
        transition_task(task, TaskStatus.ACTIVE)


def test_invalid_transition_failed_terminal() -> None:
    task = create_task()
    transition_task(task, TaskStatus.ACTIVE)
    transition_task(task, TaskStatus.FAILED)
    with pytest.raises(ValueError, match="invalid transition"):
        transition_task(task, TaskStatus.ACTIVE)


def test_invalid_transition_cancelled_terminal() -> None:
    task = create_task()
    transition_task(task, TaskStatus.CANCELLED)
    with pytest.raises(ValueError, match="invalid transition"):
        transition_task(task, TaskStatus.ACTIVE)


def test_updated_at_changes_on_transition() -> None:
    task = create_task()
    original = task.updated_at
    transition_task(task, TaskStatus.ACTIVE)
    assert task.updated_at >= original
