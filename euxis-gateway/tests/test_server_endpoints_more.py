import asyncio
import json
from types import SimpleNamespace

import pytest

fastapi = pytest.importorskip("fastapi")
if not hasattr(fastapi, "__path__"):
    pytest.skip("fastapi test client unavailable in stubbed runtime", allow_module_level=True)

from fastapi.testclient import TestClient

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


def test_health_disabled_and_unauthorized(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["health_enabled"] = False
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    resp = client.get("/health")
    assert resp.status_code == 404
    assert resp.json()["status"] == "disabled"

    config = server.load_config(None)
    config["gateway"]["health_enabled"] = True
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    resp = client.get("/health")
    assert resp.status_code == 401
    assert resp.json()["status"] == "unauthorized"


def test_cron_invalid_and_delete_not_found(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    resp = client.post("/automation/cron", json={"job_id": "job"})
    assert resp.status_code == 400

    delete = client.delete("/automation/cron/missing")
    assert delete.status_code == 404


def test_webhooks_invalid_payload(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    resp = client.post("/webhooks", json={"webhooks": "bad"})
    assert resp.status_code == 400


def test_canvas_disabled_and_invalid(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["canvas"] = {"enabled": False}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    resp = client.get("/canvas/sess")
    assert resp.status_code == 404

    config["gateway"]["canvas"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    resp = client.post("/canvas/sess", json={"state": "bad"})
    assert resp.status_code == 400
    resp = client.post("/canvas/sess/validate", json={"state": "bad"})
    assert resp.status_code == 400


def test_voice_disabled_and_invalid(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": False}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/voice/wake", json={"session_id": "s", "content": "x"}).status_code == 404
    assert client.post("/voice/talk", json={"session_id": "s", "content": "x"}).status_code == 404

    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/voice/wake", json={"session_id": "", "content": ""}).status_code == 400
    assert client.post("/voice/talk", json={"session_id": "", "content": ""}).status_code == 400


def test_voice_stt_and_tts_invalid(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "off"}}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/voice/stt", json={"session_id": "s"}).status_code == 404

    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "command", "command": "echo hi", "command_allowlist": ["echo"]}}
    monkeypatch.setattr(server, "resolve_voice_blob", lambda *_args, **_kwargs: None)
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/voice/stt", json={"session_id": "s"}).status_code == 400

    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/voice/tts", json={"session_id": "", "content": ""}).status_code == 400


def test_admin_exec_auth(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    resp = client.post("/admin/exec", json={"policy": "deny"})
    assert resp.status_code == 401

    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["admin"] = {"token": "admin"}
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    resp = client.post("/admin/exec", json={"policy": "deny"}, headers={"x-admin-token": "bad"})
    assert resp.status_code == 401
    assert any(evt["event"] == "admin_auth_failed" for evt in events)


def test_slack_and_telegram_endpoints(monkeypatch):
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

    resp = client.post("/channels/slack/events", content="{")
    assert resp.status_code == 400

    resp = client.post("/channels/slack/events", json={"type": "url_verification", "challenge": "abc"})
    assert resp.status_code == 200
    assert resp.json()["challenge"] == "abc"

    ts = str(int(server.time.time()))
    body = json.dumps({"type": "event_callback"}).encode("utf-8")
    resp = client.post(
        "/channels/slack/events",
        content=body,
        headers={"X-Slack-Request-Timestamp": ts, "X-Slack-Signature": "v0=bad"},
    )
    assert resp.status_code == 401

    resp = client.post(
        "/channels/telegram/webhook",
        json={"update_id": 1},
        headers={"x-telegram-bot-api-secret-token": "bad"},
    )
    assert resp.status_code == 401


def test_slack_and_telegram_disabled(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)
    assert client.post("/channels/slack/events", json={}).status_code == 404
    assert client.post("/channels/telegram/webhook", json={}).status_code == 404


def test_sessions_import_invalid_payload_and_broadcast(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    client = TestClient(app)

    resp = client.post("/sessions/import", json={"sessions": "bad"})
    assert resp.status_code == 400

    resp = client.post("/sessions/sess_a/broadcast", json={"message": ""})
    assert resp.status_code == 400

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.connections["conn"] = DummyWS()
    server.STATE.conn_seq["conn"] = 0
    server.STATE.conn_sessions["conn"] = "sess_a"

    resp = client.post("/sessions/sess_a/broadcast", json={"message": "hi"})
    assert resp.status_code == 200
    assert sent
