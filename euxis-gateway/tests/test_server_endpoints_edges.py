import asyncio
import base64
import json
from pathlib import Path
from types import SimpleNamespace

import pytest

fastapi = pytest.importorskip("fastapi")
if not hasattr(fastapi, "__path__"):
    pytest.skip("fastapi test client unavailable in stubbed runtime", allow_module_level=True)

from fastapi.testclient import TestClient
from starlette.requests import Request

from gateway import server


@pytest.fixture(autouse=True)
def reset_state():
    server.STATE = server.GatewayState()
    server._rate_limit_state.clear()
    server._auth_failure_state.clear()
    yield


def _build_app(monkeypatch, config, adapters=None):
    if adapters is None:
        adapters = {}
    monkeypatch.setattr(server, "build_adapters", lambda *_args, **_kwargs: adapters)
    monkeypatch.setattr(server, "load_approvals", lambda: {})
    monkeypatch.setattr(server, "load_cron_jobs", lambda: [])
    monkeypatch.setattr(server, "load_webhooks", lambda: [])
    monkeypatch.setattr(server, "load_canvas_state", lambda _sid: {})
    monkeypatch.setattr(server, "persist_canvas_state", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_cron_jobs", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_webhooks", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_approval", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "delete_approval", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "audit_log", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "load_session_from_disk", lambda _sid: [])
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {})
    monkeypatch.setattr(server, "persist_session_meta", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_message", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "load_run_events", lambda _rid: [])
    return server.build_app(config)


def test_resolve_config_env_and_args(monkeypatch, tmp_path):
    cfg = tmp_path / "cfg.json"
    cfg.write_text(json.dumps({"gateway": {"port": 1234}}), encoding="utf-8")
    monkeypatch.setenv("EUXIS_GATEWAY_CONFIG", str(cfg))
    args = SimpleNamespace(config=None, bind="0.0.0.0", port=9999, auth_mode="none")
    config = server.resolve_config(args)
    assert config["gateway"]["bind"] == "0.0.0.0"
    assert config["gateway"]["port"] == 9999
    assert config["gateway"]["auth"]["mode"] == "none"


def test_resolve_config_default(monkeypatch):
    monkeypatch.delenv("EUXIS_GATEWAY_CONFIG", raising=False)
    args = SimpleNamespace(config=None, bind=None, port=None, auth_mode=None)
    config = server.resolve_config(args)
    assert "gateway" in config


def test_verify_slack_signature_missing_headers():
    request = SimpleNamespace(headers={"X-Slack-Request-Timestamp": ""})
    assert server.verify_slack_signature(request, b"body", "secret") is False


def test_validate_audio_magic_length_edge():
    blob = b"NOPE" + b"x" * 4 + b"WAVE"
    assert server._validate_audio_magic(blob, "wav") is False


def test_unauthorized_endpoints(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    assert client.post("/automation/cron", json={}).status_code == 401
    assert client.delete("/automation/cron/job").status_code == 401
    assert client.get("/webhooks").status_code == 401
    assert client.post("/webhooks", json={}).status_code == 401
    assert client.get("/sessions").status_code == 401
    assert client.get("/canvas/sess").status_code == 401
    assert client.post("/canvas/sess", json={"state": {}}).status_code == 401
    assert client.post("/canvas/sess/validate", json={"state": {}}).status_code == 401
    assert client.post("/voice/wake", json={"session_id": "s", "content": "x"}).status_code == 401
    assert client.post("/voice/talk", json={"session_id": "s", "content": "x"}).status_code == 401
    assert client.post("/voice/tts", json={"session_id": "s", "content": "x"}).status_code == 401
    assert client.post("/voice/stt", json={"session_id": "s"}).status_code == 401
    assert client.post("/sessions/sess/broadcast", json={"message": "x"}).status_code == 401
    assert client.get("/sessions/export").status_code == 401
    assert client.post("/sessions/import", json={"sessions": {}}).status_code == 401
    assert client.get("/sessions/sess").status_code == 401
    assert client.get("/runs").status_code == 401
    assert client.get("/runs/run1").status_code == 401
    assert client.post("/approvals/run1/approve").status_code == 401
    assert client.post("/approvals/run1/reject").status_code == 401
    assert client.post("/admin/exec", json={}).status_code == 401


def test_slack_and_telegram_receive(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["channels"]["slack"]["signing_secret"] = "secret"
    config["gateway"]["channels"]["telegram"]["secret_token"] = "secret"

    class DummyAdapter:
        def __init__(self):
            self.payloads = []

        def receive(self, payload):
            self.payloads.append(payload)

    adapter = DummyAdapter()
    app = _build_app(monkeypatch, config, {"slack": adapter, "telegram": adapter})
    client = TestClient(app)

    body = json.dumps({"type": "event_callback"}).encode("utf-8")
    ts = str(int(server.time.time()))
    base = f"v0:{ts}:".encode("utf-8") + body
    digest = server.hmac.new(b"secret", base, server.hashlib.sha256).hexdigest()
    signature = f"v0={digest}"
    resp = client.post(
        "/channels/slack/events",
        content=body,
        headers={"X-Slack-Request-Timestamp": ts, "X-Slack-Signature": signature},
    )
    assert resp.status_code == 200

    resp = client.post(
        "/channels/telegram/webhook",
        json={"update_id": 1},
        headers={"x-telegram-bot-api-secret-token": "secret"},
    )
    assert resp.status_code == 200
    assert adapter.payloads


def test_telegram_webhook_direct(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["channels"]["telegram"]["secret_token"] = "secret"

    class DummyAdapter:
        def __init__(self):
            self.payloads = []

        def receive(self, payload):
            self.payloads.append(payload)

    adapter = DummyAdapter()
    app = _build_app(monkeypatch, config, {"telegram": adapter})
    endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/telegram/webhook")
    scope = {"type": "http", "headers": [(b"x-telegram-bot-api-secret-token", b"secret")], "query_string": b"", "client": ("127.0.0.2", 1234)}
    req = Request(scope)
    resp = asyncio.run(endpoint({"update_id": 2}, req))
    assert resp.status_code == 200
    assert adapter.payloads


def test_telegram_webhook_header_ok(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["channels"]["telegram"]["secret_token"] = "secret"

    class DummyAdapter:
        def receive(self, _payload):
            return None

    app = _build_app(monkeypatch, config, {"telegram": DummyAdapter()})
    client = TestClient(app)
    resp = client.post(
        "/channels/telegram/webhook",
        json={"update_id": 3},
        headers={"x-telegram-bot-api-secret-token": "secret"},
    )
    assert resp.status_code == 200


def test_telegram_webhook_header_ok_direct(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["channels"]["telegram"]["secret_token"] = "secret"
    received = []

    class DummyAdapter:
        def receive(self, payload):
            received.append(payload)

    app = _build_app(monkeypatch, config, {"telegram": DummyAdapter()})
    endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/telegram/webhook")
    scope = {"type": "http", "headers": [(b"x-telegram-bot-api-secret-token", b"secret")], "query_string": b"", "client": ("127.0.0.2", 1234)}
    req = Request(scope)
    resp = asyncio.run(endpoint({"update_id": 4}, req))
    assert resp.status_code == 200
    assert received


def test_telegram_webhook_secret_match_direct(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["channels"]["telegram"]["secret_token"] = "secret"
    received = []

    class DummyAdapter:
        def receive(self, payload):
            received.append(payload)

    app = server.build_app(config)
    server.STATE.adapters = {"telegram": DummyAdapter()}
    endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/telegram/webhook")
    scope = {"type": "http", "headers": [(b"x-telegram-bot-api-secret-token", b"secret")], "query_string": b"", "client": ("127.0.0.2", 1234)}
    req = Request(scope)
    resp = asyncio.run(endpoint({"update_id": 5}, req))
    assert resp.status_code == 200
    assert received


def test_telegram_webhook_no_secret(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["channels"]["telegram"]["secret_token"] = ""
    received = []

    class DummyAdapter:
        def receive(self, payload):
            received.append(payload)

    app = server.build_app(config)
    server.STATE.adapters = {"telegram": DummyAdapter()}
    endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/telegram/webhook")
    scope = {"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)}
    req = Request(scope)
    resp = asyncio.run(endpoint({"update_id": 6}, req))
    assert resp.status_code == 200
    assert received


def test_voice_wake_and_talk_success(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "process_inbound", lambda *_args, **_kwargs: asyncio.sleep(0))

    assert client.post("/voice/wake", json={"session_id": "s", "content": "wake"}).status_code == 200
    assert client.post("/voice/talk", json={"session_id": "s", "content": "talk"}).status_code == 200


def test_voice_upload_branches(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["voice"] = {"enabled": False}
    app = _build_app(monkeypatch, config)
    upload = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/upload")

    class DummyFile:
        def __init__(self, filename, data):
            self.filename = filename
            self._data = data

        async def stream(self):
            yield self._data

    scope = {"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)}
    req = Request(scope)
    resp = asyncio.run(upload("sess", DummyFile("audio.wav", b"RIFFxxxxWAVE1234"), req))
    assert resp.status_code == 404

    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    upload = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/upload")
    resp = asyncio.run(upload("sess", DummyFile("audio.wav", b"RIFFxxxxWAVE1234"), req))
    assert resp.status_code == 401

    scope = {"type": "http", "headers": [(b"authorization", b"Bearer tok")], "query_string": b"", "client": ("127.0.0.2", 1234)}
    req = Request(scope)
    resp = asyncio.run(upload("", DummyFile("audio.wav", b"RIFFxxxxWAVE1234"), req))
    assert resp.status_code == 400


def test_voice_tts_stt_disabled(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": False}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/voice/tts", json={"session_id": "s", "content": "x"}).status_code == 404
    assert client.post("/voice/stt", json={"session_id": "s"}).status_code == 404


def test_voice_stt_invalid_session(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "command", "command": "echo hi"}}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/voice/stt", json={"session_id": ""}).status_code == 400


def test_voice_tts_webhook(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "tts": {"mode": "webhook", "webhook_url": "http://example.com"}}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    calls = []

    async def fake_post(url, payload):
        calls.append((url, payload))

    monkeypatch.setattr(server, "post_json", fake_post)
    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))

    resp = client.post("/voice/tts", json={"session_id": "s", "content": "hello"})
    assert resp.status_code == 200
    assert calls


def test_voice_stt_transcript(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "command", "command": "echo hi"}}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    async def fake_run(*_args, **_kwargs):
        return "transcript"

    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)

    resp = client.post("/voice/stt", json={"session_id": "s", "audio_path": "file.wav"})
    assert resp.json()["content"] == "transcript"


def test_voice_tts_no_webhook(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    async def boom(*_args, **_kwargs):
        raise RuntimeError("should not post")

    monkeypatch.setattr(server, "post_json", boom)
    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))

    resp = client.post("/voice/tts", json={"session_id": "s", "content": "hello"})
    assert resp.status_code == 200


def test_voice_stt_no_transcript(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "command", "command": "echo hi"}}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    async def fake_run(*_args, **_kwargs):
        return ""

    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: (_ for _ in ()).throw(RuntimeError("no persist")))

    resp = client.post("/voice/stt", json={"session_id": "s", "audio_path": "file.wav"})
    assert resp.json()["content"] == ""


def test_voice_tts_with_session(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))
    resp = client.post("/voice/tts", json={"session_id": "s", "content": "hello"})
    assert resp.status_code == 200


def test_session_broadcast_continue(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    class DummyWS:
        async def send_text(self, _text):
            return None

    server.STATE.connections["conn"] = DummyWS()
    server.STATE.conn_sessions["conn"] = "other"
    server.STATE.conn_seq["conn"] = 0

    resp = client.post("/sessions/sess/broadcast", json={"message": "hi"})
    assert resp.status_code == 200


def test_sessions_list(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    server.STATE.sessions["sess"] = [{"role": "user"}]
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    resp = client.get("/sessions")
    assert resp.json()["sessions"][0]["message_count"] == 1


def test_canvas_disabled_post(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["canvas"] = {"enabled": False}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/canvas/sess", json={"state": {}}).status_code == 404
    assert client.post("/canvas/sess/validate", json={"state": {}}).status_code == 404


def test_session_detail_loads_disk(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    server.STATE.sessions["sess"] = []
    monkeypatch.setattr(server, "load_session_from_disk", lambda _sid: [{"role": "user"}])
    app = server.build_app(config)
    client = TestClient(app)
    resp = client.get("/sessions/sess")
    assert resp.json()["messages"][0]["role"] == "user"


def test_session_detail_no_reload(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    server.STATE.sessions["sess"] = [{"role": "assistant"}]
    monkeypatch.setattr(server, "load_session_from_disk", lambda _sid: (_ for _ in ()).throw(RuntimeError("no reload")))
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"session_id": "sess"})
    app = server.build_app(config)
    client = TestClient(app)
    resp = client.get("/sessions/sess")
    assert resp.json()["messages"][0]["role"] == "assistant"


def test_approvals_not_found(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    assert client.post("/approvals/missing/approve").status_code == 404
    assert client.post("/approvals/missing/reject").status_code == 404


def test_admin_exec_with_token(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["admin"] = {"token": "admin"}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    resp = client.post("/admin/exec", json={"policy": "deny"}, headers={"x-admin-token": "admin"})
    assert resp.status_code == 200
