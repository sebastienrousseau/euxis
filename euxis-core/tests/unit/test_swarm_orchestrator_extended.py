# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import json

import pytest

from euxis_core.mesh.swarm import SwarmOrchestrator, SwarmTask


class _FakeRouter:
    def __init__(self, provider: str = "groq") -> None:
        self.provider = provider
        self.usage: list[tuple[str, int]] = []

    def select_provider(self, _complexity: str) -> str:
        return self.provider

    def track_usage(self, provider: str, tokens: int) -> None:
        self.usage.append((provider, tokens))


class _FakeWS:
    def __init__(self, messages: list[str] | None = None) -> None:
        self.messages = messages or []
        self.sent: list[str] = []
        self._closed = True

    @property
    def closed(self) -> bool:
        return self._closed

    async def connect(self) -> None:
        self._closed = False

    async def send(self, payload: str) -> None:
        self.sent.append(payload)

    async def close(self) -> None:
        self._closed = True

    async def iter_messages(self):
        for msg in self.messages:
            yield msg

    @staticmethod
    def invalid_status_code(_exc: Exception):
        return None


@pytest.mark.asyncio
async def test_ensure_connection_reports_auth_failure(monkeypatch) -> None:
    orchestrator = SwarmOrchestrator("http://gateway.example")
    orchestrator.simulation_mode = False

    class AuthFailClient(_FakeWS):
        async def connect(self) -> None:
            raise RuntimeError("unauthorized")

        @staticmethod
        def invalid_status_code(_exc: Exception):
            return 401

    orchestrator.ws_client = AuthFailClient()
    with pytest.raises(RuntimeError):
        await orchestrator._ensure_connection()


@pytest.mark.asyncio
async def test_ensure_connection_reports_generic_failure(monkeypatch) -> None:
    orchestrator = SwarmOrchestrator("http://gateway.example")
    orchestrator.simulation_mode = False

    class FailClient(_FakeWS):
        async def connect(self) -> None:
            raise RuntimeError("network down")

    orchestrator.ws_client = FailClient()
    with pytest.raises(RuntimeError):
        await orchestrator._ensure_connection()


@pytest.mark.asyncio
async def test_ensure_connection_noop_when_already_connected() -> None:
    orchestrator = SwarmOrchestrator("http://gateway.example")
    orchestrator.simulation_mode = False

    class ConnectedClient(_FakeWS):
        @property
        def closed(self) -> bool:
            return False

        async def connect(self) -> None:  # pragma: no cover - should not be called
            raise AssertionError("connect should not run when already connected")

    orchestrator.ws_client = ConnectedClient()
    await orchestrator._ensure_connection()


@pytest.mark.asyncio
async def test_run_task_non_simulation_success_with_usage() -> None:
    orchestrator = SwarmOrchestrator("http://gateway.example")
    orchestrator.simulation_mode = False
    orchestrator.router = _FakeRouter("openai")

    task = SwarmTask("architect", "build x")
    final_event = json.dumps(
        {
            "type": "event",
            "event": "agent",
            "data": {
                "run_id": task.run_id,
                "status": "final",
                "content": "done",
                "meta": {"usage": {"total_tokens": 777}},
            },
        }
    )
    ws = _FakeWS(messages=[final_event])
    orchestrator.ws_client = ws

    await orchestrator._run_task(task)

    assert task.status == "completed"
    assert task.result == "done"
    assert len(ws.sent) == 1
    sent = json.loads(ws.sent[0])
    assert sent["id"] == task.run_id
    assert sent["params"]["meta"]["provider"] == "openai"
    assert orchestrator.router.usage == [("openai", 777)]
    assert orchestrator.history[-1]["result"] == "done"


@pytest.mark.asyncio
async def test_run_task_non_simulation_handles_intermediate_frames() -> None:
    orchestrator = SwarmOrchestrator("http://gateway.example")
    orchestrator.simulation_mode = False
    orchestrator.router = _FakeRouter("openai")

    task = SwarmTask("architect", "build x")
    ws = _FakeWS(
        messages=[
            json.dumps({"type": "result", "id": task.run_id, "ok": True}),
            json.dumps(
                {
                    "type": "event",
                    "event": "agent",
                    "data": {"run_id": "other_run", "status": "final", "content": "ignore"},
                }
            ),
            json.dumps(
                {
                    "type": "event",
                    "event": "agent",
                    "data": {"run_id": task.run_id, "status": "running", "content": "still working"},
                }
            ),
            json.dumps(
                {
                    "type": "event",
                    "event": "agent",
                    "data": {"run_id": task.run_id, "status": "final", "content": "done"},
                }
            ),
        ]
    )
    orchestrator.ws_client = ws

    await orchestrator._run_task(task)
    assert task.status == "completed"
    assert task.result == "done"


@pytest.mark.asyncio
async def test_run_task_non_simulation_result_error() -> None:
    orchestrator = SwarmOrchestrator("http://gateway.example")
    orchestrator.simulation_mode = False
    orchestrator.router = _FakeRouter("groq")

    task = SwarmTask("reviewer", "check x")
    ws = _FakeWS(
        messages=[
            json.dumps(
                {
                    "type": "result",
                    "id": task.run_id,
                    "error": {"code": "BAD", "message": "nope"},
                }
            )
        ]
    )
    orchestrator.ws_client = ws

    await orchestrator._run_task(task)
    assert task.status == "failed"
    assert "Gateway error" in (task.result or "")
    assert orchestrator.history[-1]["agent"] == "reviewer"


@pytest.mark.asyncio
async def test_run_task_non_simulation_agent_error() -> None:
    orchestrator = SwarmOrchestrator("http://gateway.example")
    orchestrator.simulation_mode = False
    orchestrator.router = _FakeRouter("anthropic")

    task = SwarmTask("planner", "plan x")
    ws = _FakeWS(
        messages=[
            json.dumps(
                {
                    "type": "event",
                    "event": "agent",
                    "data": {"run_id": task.run_id, "status": "error", "content": "boom"},
                }
            )
        ]
    )
    orchestrator.ws_client = ws

    await orchestrator._run_task(task)
    assert task.status == "failed"
    assert "Agent error" in (task.result or "")


@pytest.mark.asyncio
async def test_run_task_non_simulation_missing_final_event() -> None:
    orchestrator = SwarmOrchestrator("http://gateway.example")
    orchestrator.simulation_mode = False
    orchestrator.router = _FakeRouter("groq")
    task = SwarmTask("writer", "write x")
    orchestrator.ws_client = _FakeWS(messages=[json.dumps({"type": "event", "event": "tick"})])

    await orchestrator._run_task(task)
    assert task.status == "failed"
    assert "WebSocket closed before task completion" in (task.result or "")
