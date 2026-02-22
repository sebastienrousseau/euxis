import asyncio
import json
from types import SimpleNamespace

import pytest

from gateway import server


def test_handle_frame_variants(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    ws = DummyWS()
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"

    monkeypatch.setattr(server, "load_session_from_disk", lambda session_id: [])
    monkeypatch.setattr(server, "persist_session_meta", lambda session_id, meta: None)
    monkeypatch.setattr(server, "persist_approval", lambda run_id, entry: None)
    monkeypatch.setattr(server, "audit_log", lambda payload: None)

    seq = {"value": 0}

    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "c1",
        "method": "gateway.connect",
        "params": {"protocol": "v0.1"},
    }), seq, config, "conn"))
    assert sent[-1]["type"] == "event"

    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "h1",
        "method": "chat.history",
        "params": {"session_id": "sess", "limit": 1},
    }), seq, config, "conn"))
    assert sent[-1]["ok"] is True

    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "i1",
        "method": "chat.inject",
        "params": {"session_id": "sess", "role": "assistant", "content": "hi"},
    }), seq, config, "conn"))
    assert sent[-1]["ok"] is True

    server.STATE.running["run_abort"] = SimpleNamespace(cancel=lambda: None)
    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "a1",
        "method": "chat.abort",
        "params": {"run_id": "run_abort"},
    }), seq, config, "conn"))
    assert sent[-1]["ok"] is True

    config["gateway"]["policy"]["mention_required"] = True
    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "s1",
        "method": "chat.send",
        "params": {
            "session_id": "sess",
            "role": "user",
            "content": "blocked",
            "meta": {"mentions_bot": False, "scope": "channel", "sender": "user"},
        },
    }), seq, config, "conn"))
    assert sent[-1]["error"]["code"] == "POLICY_BLOCKED"

    config["gateway"]["policy"]["mention_required"] = False
    config["gateway"]["exec"]["ask"] = "always"
    config["gateway"]["exec"]["ask_fallback"] = "deny"
    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "s2",
        "method": "chat.send",
        "params": {
            "session_id": "sess",
            "role": "user",
            "content": "needs approval",
            "meta": {"agent": "a"},
        },
    }), seq, config, "conn"))
    assert sent[-1]["error"]["code"] == "APPROVAL_REQUIRED"


def test_dispatch_agent_success(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    class DummyStream:
        def __init__(self, lines):
            self._lines = lines

        def __aiter__(self):
            return self

        async def __anext__(self):
            if not self._lines:
                raise StopAsyncIteration
            return self._lines.pop(0)

    class DummyProc:
        def __init__(self):
            self.stdout = DummyStream([b"hello\n"])
            self.stderr = DummyStream([b""])
            self.returncode = 0

        async def wait(self):
            return 0

    async def fake_exec(*_args, **_kwargs):
        return DummyProc()

    monkeypatch.setattr(asyncio, "create_subprocess_exec", fake_exec)
    monkeypatch.setattr(server, "persist_run_event", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "notify_webhooks", lambda *_args, **_kwargs: asyncio.sleep(0))
    monkeypatch.setattr(server, "deliver_to_channel", lambda *_args, **_kwargs: asyncio.sleep(0))
    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))

    ws = DummyWS()
    asyncio.run(server.dispatch_agent(ws, {"value": 0}, "sess", "run", {"agent": "a"}, "hi", server.load_config(None)))
    assert any(event["data"]["status"] == "final" for event in sent)


def test_dispatch_agent_error(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    async def fake_exec(*_args, **_kwargs):
        raise RuntimeError("boom")

    monkeypatch.setattr(asyncio, "create_subprocess_exec", fake_exec)

    ws = DummyWS()
    asyncio.run(server.dispatch_agent(ws, {"value": 0}, "sess", "run", {"agent": "a"}, "hi", server.load_config(None)))
    assert sent[-1]["data"]["status"] == "error"


def test_dispatch_agent_modes(monkeypatch):
    captured = []

    class DummyWS:
        async def send_text(self, _text):
            return None

    class DummyProc:
        def __init__(self):
            self.stdout = DummyStream([b"\n", b"line\n"])
            self.stderr = DummyStream([b"\n", b"err\n"])
            self.returncode = 0

        async def wait(self):
            return 0

    class DummyStream:
        def __init__(self, lines):
            self._lines = list(lines)

        def __aiter__(self):
            return self

        async def __anext__(self):
            if not self._lines:
                raise StopAsyncIteration
            return self._lines.pop(0)

    async def fake_exec(*args, **_kwargs):
        captured.append(args)
        return DummyProc()

    monkeypatch.setattr(asyncio, "create_subprocess_exec", fake_exec)
    monkeypatch.setattr(server, "persist_run_event", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "notify_webhooks", lambda *_args, **_kwargs: asyncio.sleep(0))
    monkeypatch.setattr(server, "deliver_to_channel", lambda *_args, **_kwargs: asyncio.sleep(0))
    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))

    asyncio.run(server.dispatch_agent(DummyWS(), {"value": 0}, "sess", "run", {"mode": "squad", "squad": "build"}, "hi", server.load_config(None)))
    asyncio.run(server.dispatch_agent(DummyWS(), {"value": 0}, "sess", "run", {"mode": "combo", "combo": "envision"}, "hi", server.load_config(None)))
    asyncio.run(server.dispatch_agent(DummyWS(), {"value": 0}, "sess", "run", {"mode": "playbook", "playbook": "zero"}, "hi", server.load_config(None)))
    asyncio.run(server.dispatch_agent(DummyWS(), {"value": 0}, "sess", "run", {"agent": "a", "provider": "x"}, "hi", server.load_config(None)))
    assert captured


def test_send_ticks_cancel(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    async def fake_sleep(_):
        raise asyncio.CancelledError

    monkeypatch.setattr(asyncio, "sleep", fake_sleep)
    asyncio.run(server.send_ticks(DummyWS(), {"value": 0}))
    assert sent == []


def test_cron_loop_cancel(monkeypatch):
    async def fake_sleep(_):
        raise asyncio.CancelledError

    monkeypatch.setattr(asyncio, "sleep", fake_sleep)
    server.STATE.cron_jobs = [{"job_id": "j", "every_seconds": 1, "session_id": "sess", "content": "hi"}]
    monkeypatch.setattr(server, "persist_cron_jobs", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "cleanup_voice", lambda *_args, **_kwargs: 0)
    monkeypatch.setattr(server, "process_inbound", lambda *_args, **_kwargs: asyncio.sleep(0))
    asyncio.run(server.cron_loop(server.load_config(None)))


def test_notify_webhooks(monkeypatch):
    server.STATE.webhooks = [{"url": "http://example.com", "events": ["agent.final"]}]

    class DummyResp:
        def __enter__(self):
            return self

        def __exit__(self, *args):
            return False

        def read(self):
            return b""

    def fake_urlopen(_req, timeout=None):
        return DummyResp()

    monkeypatch.setattr(server.urlrequest, "urlopen", fake_urlopen)
    asyncio.run(server.notify_webhooks({"event": "agent", "data": {"status": "final"}}))


def test_post_json_error(monkeypatch):
    def boom(*_args, **_kwargs):
        raise server.urlerror.URLError("bad")

    monkeypatch.setattr(server.urlrequest, "urlopen", boom)
    asyncio.run(server.post_json("http://example.com", {"ok": True}))

def test_handle_frame_chat_send_success(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    ws = DummyWS()
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["policy"]["mention_required"] = False
    config["gateway"]["exec"]["ask"] = "off"
    config["gateway"]["exec"]["ask_fallback"] = "full"

    monkeypatch.setattr(server, "load_session_meta", lambda session_id: {})
    monkeypatch.setattr(server, "persist_session_meta", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_message", lambda *_args, **_kwargs: None)

    async def fake_dispatch(*_args, **_kwargs):
        return None

    monkeypatch.setattr(server, "dispatch_with_lock", fake_dispatch)

    seq = {"value": 0}
    asyncio.run(server.handle_frame(ws, json.dumps({
        "type": "request",
        "id": "send1",
        "method": "chat.send",
        "params": {
            "session_id": "sess",
            "role": "user",
            "content": "ok",
            "meta": {"agent": "a"},
        },
    }), seq, config, "conn"))

    assert sent[-1]["ok"] is True
