"""A2A protocol task types and state management."""

from __future__ import annotations

import time
import uuid
from dataclasses import dataclass, field
from typing import Any


class TaskStatus:
    PENDING: str = "pending"
    ACTIVE: str = "active"
    COMPLETED: str = "completed"
    FAILED: str = "failed"
    CANCELLED: str = "cancelled"


_VALID_TRANSITIONS: dict[str, set[str]] = {
    TaskStatus.PENDING: {TaskStatus.ACTIVE, TaskStatus.CANCELLED},
    TaskStatus.ACTIVE: {TaskStatus.COMPLETED, TaskStatus.FAILED, TaskStatus.CANCELLED},
    TaskStatus.COMPLETED: set(),
    TaskStatus.FAILED: set(),
    TaskStatus.CANCELLED: set(),
}


@dataclass(slots=True)
class A2ATask:
    id: str
    status: str
    messages: list[Any] = field(default_factory=list)
    artifacts: list[Any] = field(default_factory=list)
    history: list[dict[str, str]] = field(default_factory=list)
    created_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)


def create_task(message: Any = None) -> A2ATask:
    task = A2ATask(id=uuid.uuid4().hex, status=TaskStatus.PENDING)
    if message is not None:
        task.messages.append(message)
    return task


def transition_task(task: A2ATask, new_status: str) -> A2ATask:
    valid = _VALID_TRANSITIONS.get(task.status, set())
    if new_status not in valid:
        raise ValueError(
            f"invalid transition: {task.status} -> {new_status}"
        )
    task.history.append({"from": task.status, "to": new_status})
    task.status = new_status
    task.updated_at = time.time()
    return task
