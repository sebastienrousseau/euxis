import asyncio
import json
import types
from types import SimpleNamespace

import pytest

from gateway import server


def test_handle_frame_invalid_json(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    ws = DummyWS()
    asyncio.run(server.handle_frame(ws, "{bad", {"value": 0}, server.load_config(None), "conn"))
    assert sent[0]["error"]["code"] == "INVALID_REQUEST"


def test_handle_frame_non_request(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    ws = DummyWS()
    asyncio.run(server.handle_frame(ws, json.dumps({"type": "not_request"}), {"value": 0}, server.load_config(None), "conn"))
    assert sent[0]["error"]["message"] == "Expected request frame"


def test_handle_frame_unknown_method(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    ws = DummyWS()
    asyncio.run(server.handle_frame(ws, json.dumps({"type": "request", "id": "1", "method": "nope", "params": {}}), {"value": 0}, server.load_config(None), "conn"))
    assert sent[0]["error"]["code"] == "INVALID_REQUEST"


def test_handle_frame_missing_fields():
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    ws = DummyWS()
    config = server.load_config(None)

    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "c1",
        "method": "gateway.connect",
        "params": {},
    }), {"value": 0}, config, "conn"))
    assert sent[-1]["error"]["message"] == "Missing protocol"

    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "h1",
        "method": "chat.history",
        "params": {},
    }), {"value": 0}, config, "conn"))
    assert sent[-1]["error"]["message"] == "Missing session_id"

    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "s1",
        "method": "chat.send",
        "params": {"session_id": "sess"},
    }), {"value": 0}, config, "conn"))
    assert sent[-1]["error"]["message"] == "Missing session_id or role"

    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "i1",
        "method": "chat.inject",
        "params": {"session_id": "sess"},
    }), {"value": 0}, config, "conn"))
    assert sent[-1]["error"]["message"] == "Missing session_id or role"


def test_handle_frame_history_loads_disk(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    monkeypatch.setattr(server, "load_session_from_disk", lambda _sid: [{"role": "user"}])
    ws = DummyWS()
    config = server.load_config(None)
    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "h1",
        "method": "chat.history",
        "params": {"session_id": "sess"},
    }), {"value": 0}, config, "conn"))
    assert sent[-1]["result"]["messages"][0]["role"] == "user"


def test_handle_frame_history_no_reload(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.sessions["sess"] = [{"role": "assistant"}]
    monkeypatch.setattr(server, "load_session_from_disk", lambda _sid: (_ for _ in ()).throw(RuntimeError("should not load")))
    ws = DummyWS()
    config = server.load_config(None)
    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "h1",
        "method": "chat.history",
        "params": {"session_id": "sess"},
    }), {"value": 0}, config, "conn"))
    assert sent[-1]["result"]["messages"][0]["role"] == "assistant"


def test_handle_frame_abort_with_task(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.running["run_abort"] = SimpleNamespace(cancel=lambda: None)
    ws = DummyWS()
    config = server.load_config(None)
    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "a1",
        "method": "chat.abort",
        "params": {"run_id": "run_abort"},
    }), {"value": 0}, config, "conn"))
    assert sent[-1]["result"]["aborted"] is True


def test_handle_frame_abort_without_task():
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    ws = DummyWS()
    config = server.load_config(None)
    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "a1",
        "method": "chat.abort",
        "params": {"run_id": "missing"},
    }), {"value": 0}, config, "conn"))
    assert sent[-1]["result"]["aborted"] is True

def test_dispatch_with_lock_no_adapter(monkeypatch):
    server.STATE.sessions = {"sess": []}
    server.STATE.session_locks = {"sess": asyncio.Lock()}
    server.STATE.adapters = {}

    class DummyWS:
        async def send_text(self, _text):
            return None

    async def run():
        await server.dispatch_with_lock(DummyWS(), {"value": 0}, "sess", "run", {"agent": "a"}, "hi", server.load_config(None))

    asyncio.run(run())
