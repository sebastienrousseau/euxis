import asyncio
import base64
import json
from pathlib import Path

import pytest

fastapi = pytest.importorskip("fastapi")
if not hasattr(fastapi, "__path__"):
    pytest.skip("real fastapi not available", allow_module_level=True)

from gateway import server


def _get_ws_endpoint(app, path):
    for route in app.routes:
        if getattr(route, "path", None) == path:
            return route.endpoint
    raise AssertionError("websocket endpoint not found")


class FakeWS:
    def __init__(self, messages):
        self._messages = list(messages)
        self.accepted = False
        self.closed = False
        self.sent = []

    async def accept(self):
        self.accepted = True

    async def receive(self):
        if not self._messages:
            raise server.WebSocketDisconnect()
        return self._messages.pop(0)

    async def send_text(self, text):
        self.sent.append(json.loads(text))

    async def close(self, code=1000):
        self.closed = True
        self.close_code = code


def test_voice_stream_disabled():
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": False}
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")
    ws = FakeWS([])
    asyncio.run(endpoint(ws))
    assert ws.accepted is True
    assert ws.closed is True
    assert ws.sent[0]["error"] == "voice_disabled"


def test_voice_stream_flow(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "tts": {"mode": "webhook", "webhook_url": "http://example.com"},
        "stt": {"mode": "command", "command": "echo hi", "auto_inject": True},
        "command_allowlist": ["echo"],
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    calls = {"append": 0, "persist": 0, "post": 0, "process": 0}

    monkeypatch.setattr(server, "append_voice_chunk", lambda *_args, **_kwargs: calls.__setitem__("append", calls["append"] + 1))
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: calls.__setitem__("persist", calls["persist"] + 1))
    monkeypatch.setattr(server, "post_json", lambda *_args, **_kwargs: asyncio.sleep(0))

    async def fake_run_voice(*_args, **_kwargs):
        return "transcript"

    monkeypatch.setattr(server, "run_voice_command", fake_run_voice)

    async def fake_process(*_args, **_kwargs):
        calls["process"] += 1

    monkeypatch.setattr(server, "process_inbound", fake_process)
    monkeypatch.setattr(server, "resolve_voice_blob", lambda *_args, **_kwargs: None)

    messages = [
        {"bytes": b"123"},
        {"binary": b"skip"},
        {"text": json.dumps({"event": "chunk", "data": base64.b64encode(b"abc").decode("utf-8")})},
        {"text": json.dumps({"event": "chunk", "data": 123})},
        {"text": "not-json"},
        {"text": json.dumps({"event": "start"})},
        {"text": json.dumps({"event": "start", "session_id": "sess", "meta": {"m": 1}})},
        {"bytes": b"abc"},
        {"text": json.dumps({"event": "chunk", "data": ""})},
        {"text": json.dumps({"event": "chunk", "data": "@@@"})},
        {"text": json.dumps({"event": "chunk", "data": 123})},
        {"text": json.dumps({"event": "chunk", "data": base64.b64encode(b"abc").decode("utf-8")})},
        {"text": json.dumps({"event": "stt", "content": ""})},
        {"text": json.dumps({"event": "stt", "content": "hello", "meta": {"m": 1}})},
        {"text": json.dumps({"event": "tts", "content": ""})},
        {"text": json.dumps({"event": "tts", "content": "speak"})},
        {"text": json.dumps({"event": "text", "content": ""})},
        {"text": json.dumps({"event": "text", "content": "run", "meta": {"agent": "a"}})},
        {"text": json.dumps({"event": "end"})},
        {"text": json.dumps({"event": "unknown"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert ws.accepted is True
    assert calls["append"] >= 1
    assert calls["persist"] >= 2


def test_voice_stream_end_webhook(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "stt": {"mode": "webhook", "webhook_url": "http://example.com"},
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    posts = []

    async def fake_post(url, payload):
        posts.append((url, payload))

    monkeypatch.setattr(server, "post_json", fake_post)
    monkeypatch.setattr(server, "append_voice_chunk", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "resolve_voice_blob", lambda *_args, **_kwargs: Path("/tmp/audio.raw"))

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
        {"bytes": b"abc"},
        {"text": json.dumps({"event": "end"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert posts
    assert "sess" not in server.STATE.voice_connections or ws not in server.STATE.voice_connections.get("sess", [])


def test_voice_stream_end_command(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "stt": {"mode": "command", "command": "echo hi", "auto_inject": True},
        "command_allowlist": ["echo"],
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    seen = {"process": 0, "persist": 0}

    monkeypatch.setattr(server, "resolve_voice_blob", lambda *_args, **_kwargs: Path("/tmp/audio.raw"))

    async def fake_run(*_args, **_kwargs):
        return "transcript"

    async def fake_process(*_args, **_kwargs):
        seen["process"] += 1

    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: seen.__setitem__("persist", seen["persist"] + 1))
    monkeypatch.setattr(server.asyncio, "create_task", lambda coro: coro.close() or None)

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
        {"bytes": b"abc"},
        {"text": json.dumps({"event": "end"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert seen["persist"] >= 1


def test_voice_stream_end_no_audio(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    calls = []

    async def fake_post(*_args, **_kwargs):
        calls.append("post")

    monkeypatch.setattr(server, "post_json", fake_post)

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
        {"text": json.dumps({"event": "end"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert not calls


def test_voice_stream_tts_webhook(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "tts": {"mode": "webhook", "webhook_url": "http://example.com"},
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    posts = []

    async def fake_post(url, payload):
        posts.append((url, payload))

    monkeypatch.setattr(server, "post_json", fake_post)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
        {"text": json.dumps({"event": "tts", "session_id": "sess", "content": "hello"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert posts


def test_voice_stream_tts_webhook_direct(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "tts": {"mode": "webhook", "webhook_url": "http://example.com"},
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    posts = []

    async def fake_post(url, payload):
        posts.append((url, payload))

    monkeypatch.setattr(server, "post_json", fake_post)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
        {"text": json.dumps({"event": "tts", "session_id": "sess", "content": "hello"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert posts


def test_voice_stream_tts_webhook_no_start(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "tts": {"mode": "webhook", "webhook_url": "http://example.com"},
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    posts = []

    async def fake_post(url, payload):
        posts.append((url, payload))

    monkeypatch.setattr(server, "post_json", fake_post)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)

    messages = [
        {"text": json.dumps({"event": "tts", "session_id": "sess", "content": "hello"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert posts


def test_voice_stream_tts_webhook_raises(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "tts": {"mode": "webhook", "webhook_url": "http://example.com"},
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    async def boom(_url, _payload):
        raise RuntimeError("hit")

    monkeypatch.setattr(server, "post_json", boom)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)

    messages = [
        {"text": json.dumps({"event": "tts", "session_id": "sess", "content": "hello"})},
    ]

    ws = FakeWS(messages)
    with pytest.raises(RuntimeError):
        asyncio.run(endpoint(ws))


def test_voice_stream_tts_no_webhook(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "tts": {"mode": "command", "command": "echo hi"}}
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    posts = []

    async def fake_post(*_args, **_kwargs):
        posts.append("post")

    monkeypatch.setattr(server, "post_json", fake_post)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
        {"text": json.dumps({"event": "tts", "session_id": "sess", "content": "hello"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert not posts
    assert any(msg.get("type") == "ack" and msg.get("event") == "tts" for msg in ws.sent)


def test_voice_stream_disconnect_clears_connection(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
    assert "sess" not in server.STATE.voice_connections


def test_voice_stream_end_auto_inject_off(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "stt": {"mode": "command", "command": "echo hi", "auto_inject": False},
        "command_allowlist": ["echo"],
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    monkeypatch.setattr(server, "resolve_voice_blob", lambda *_args, **_kwargs: Path("/tmp/audio.raw"))

    async def fake_run(*_args, **_kwargs):
        return "transcript"

    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server.asyncio, "create_task", lambda coro: (_ for _ in ()).throw(RuntimeError("no task")))

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
        {"bytes": b"abc"},
        {"text": json.dumps({"event": "end"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))


def test_voice_stream_disconnect_cleanup(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
    ]

    ws = FakeWS(messages)
    server.STATE.voice_connections["sess"] = [ws]
    asyncio.run(endpoint(ws))
    assert "sess" not in server.STATE.voice_connections


def test_voice_stream_disconnect_keeps_other(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
    ]

    ws = FakeWS(messages)
    other = FakeWS([])
    server.STATE.voice_connections["sess"] = [ws, other]
    asyncio.run(endpoint(ws))
    assert server.STATE.voice_connections["sess"] == [other]


def test_voice_stream_end_no_transcript(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "stt": {"mode": "command", "command": "echo hi", "auto_inject": True},
        "command_allowlist": ["echo"],
    }
    app = server.build_app(config)
    endpoint = _get_ws_endpoint(app, "/voice/stream")

    monkeypatch.setattr(server, "resolve_voice_blob", lambda *_args, **_kwargs: Path("/tmp/audio.raw"))

    async def fake_run(*_args, **_kwargs):
        return ""

    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: (_ for _ in ()).throw(RuntimeError("no persist")))

    messages = [
        {"text": json.dumps({"event": "start", "session_id": "sess"})},
        {"bytes": b"abc"},
        {"text": json.dumps({"event": "end"})},
    ]

    ws = FakeWS(messages)
    asyncio.run(endpoint(ws))
