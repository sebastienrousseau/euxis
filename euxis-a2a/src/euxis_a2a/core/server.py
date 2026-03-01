"""A2A server handler for processing incoming requests."""

from __future__ import annotations

from typing import Any

from euxis_a2a.core.agent_card import AgentCard, serialize_card
from euxis_a2a.core.task import A2ATask, TaskStatus, create_task, transition_task


class A2AServerHandler:
    """Handles A2A JSON-RPC style requests."""

    def __init__(self, card: AgentCard) -> None:
        self._card = card
        self._tasks: dict[str, A2ATask] = {}

    def handle_request(self, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        params = params or {}
        if method == "agent_card":
            return serialize_card(self._card)
        if method == "task_create":
            task = create_task(params.get("message"))
            self._tasks[task.id] = task
            return {"task_id": task.id, "status": task.status}
        if method == "task_get":
            task_id = params.get("task_id", "")
            task = self._tasks.get(task_id)
            if task is None:
                return {"error": "task_not_found", "task_id": task_id}
            return {"task_id": task.id, "status": task.status, "messages": len(task.messages)}
        if method == "task_cancel":
            task_id = params.get("task_id", "")
            task = self._tasks.get(task_id)
            if task is None:
                return {"error": "task_not_found", "task_id": task_id}
            try:
                transition_task(task, TaskStatus.CANCELLED)
                return {"task_id": task.id, "status": task.status}
            except ValueError as exc:
                return {"error": str(exc)}
        if method == "capabilities_list":
            return {"capabilities": [c.name for c in self._card.capabilities]}
        return {"error": "unknown_method", "method": method}
