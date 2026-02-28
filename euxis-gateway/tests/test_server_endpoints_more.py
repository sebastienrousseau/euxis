import asyncio
import json
from types import SimpleNamespace

import pytest

fastapi = pytest.importorskip("fastapi")
if not hasattr(fastapi, "__path__"):
    pytest.skip("fastapi test client unavailable in stubbed runtime", allow_module_level=True)

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


def test_health_disabled_and_unauthorized(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["health_enabled"] = False
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    health = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/health")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(health(req))
    assert resp.status_code == 404
    assert json.loads(resp.body.decode("utf-8"))["status"] == "disabled"

    config = server.load_config(None)
    config["gateway"]["health_enabled"] = True
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    app = _build_app(monkeypatch, config)
    health = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/health")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(health(req))
    assert resp.status_code == 401
    assert json.loads(resp.body.decode("utf-8"))["status"] == "unauthorized"


def test_cron_invalid_and_delete_not_found(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    cron_create = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/automation/cron" and "POST" in (getattr(route, "methods", set()) or set())
    )
    cron_delete = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/automation/cron/{job_id}" and "DELETE" in (getattr(route, "methods", set()) or set())
    )
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(cron_create({"job_id": "job"}, req)).status_code == 400
    assert asyncio.run(cron_delete("missing", req)).status_code == 404


def test_webhooks_invalid_payload(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    webhooks_set = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/webhooks" and "POST" in (getattr(route, "methods", set()) or set())
    )
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(webhooks_set({"webhooks": "bad"}, req))
    assert resp.status_code == 400


def test_canvas_disabled_and_invalid(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["canvas"] = {"enabled": False}
    app = _build_app(monkeypatch, config)
    canvas_get = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/canvas/{session_id}" and "GET" in (getattr(route, "methods", set()) or set())
    )
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(canvas_get("sess", req))
    assert resp.status_code == 404

    config["gateway"]["canvas"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    canvas_set = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/canvas/{session_id}" and "POST" in (getattr(route, "methods", set()) or set())
    )
    canvas_validate = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/canvas/{session_id}/validate" and "POST" in (getattr(route, "methods", set()) or set())
    )
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(canvas_set("sess", {"state": "bad"}, req)).status_code == 400
    assert asyncio.run(canvas_validate("sess", {"state": "bad"}, req)).status_code == 400


def test_voice_disabled_and_invalid(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": False}
    app = _build_app(monkeypatch, config)
    wake = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/wake")
    talk = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/talk")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(wake({"session_id": "s", "content": "x"}, req)).status_code == 404
    assert asyncio.run(talk({"session_id": "s", "content": "x"}, req)).status_code == 404

    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    wake = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/wake")
    talk = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/talk")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(wake({"session_id": "", "content": ""}, req)).status_code == 400
    assert asyncio.run(talk({"session_id": "", "content": ""}, req)).status_code == 400


def test_voice_stt_and_tts_invalid(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "off"}}
    app = _build_app(monkeypatch, config)
    stt = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/stt")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(stt({"session_id": "s"}, req)).status_code == 404

    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "command", "command": "echo hi", "command_allowlist": ["echo"]}}
    monkeypatch.setattr(server, "resolve_voice_blob", lambda *_args, **_kwargs: None)
    app = _build_app(monkeypatch, config)
    stt = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/stt")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(stt({"session_id": "s"}, req)).status_code == 400

    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    tts = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/tts")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(tts({"session_id": "", "content": ""}, req)).status_code == 400




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
    slack = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/slack/events")
    telegram = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/telegram/webhook")

    first = True
    async def receive_invalid():
        nonlocal first
        if first:
            first = False
            return {"type": "http.request", "body": b"{", "more_body": False}
        return {"type": "http.disconnect"}
    req = Request({"type": "http", "method": "POST", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)}, receive=receive_invalid)
    assert asyncio.run(slack(req)).status_code == 400

    body = json.dumps({"type": "url_verification", "challenge": "abc"}).encode("utf-8")
    first = True
    async def receive_challenge():
        nonlocal first
        if first:
            first = False
            return {"type": "http.request", "body": body, "more_body": False}
        return {"type": "http.disconnect"}
    req = Request({"type": "http", "method": "POST", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)}, receive=receive_challenge)
    resp = asyncio.run(slack(req))
    assert resp.status_code == 200
    assert json.loads(resp.body.decode("utf-8"))["challenge"] == "abc"

    ts = str(int(server.time.time()))
    body = json.dumps({"type": "event_callback"}).encode("utf-8")
    first = True
    async def receive_signed():
        nonlocal first
        if first:
            first = False
            return {"type": "http.request", "body": body, "more_body": False}
        return {"type": "http.disconnect"}
    req = Request(
        {
            "type": "http",
            "method": "POST",
            "headers": [
                (b"x-slack-request-timestamp", ts.encode("utf-8")),
                (b"x-slack-signature", b"v0=bad"),
            ],
            "query_string": b"",
            "client": ("127.0.0.2", 1234),
        },
        receive=receive_signed,
    )
    assert asyncio.run(slack(req)).status_code == 401

    req = Request(
        {
            "type": "http",
            "method": "POST",
            "headers": [(b"x-telegram-bot-api-secret-token", b"bad")],
            "query_string": b"",
            "client": ("127.0.0.2", 1234),
        }
    )
    assert asyncio.run(telegram({"update_id": 1}, req)).status_code == 401


def test_slack_and_telegram_disabled(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    slack = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/slack/events")
    telegram = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/telegram/webhook")
    first = True
    async def receive_empty():
        nonlocal first
        if first:
            first = False
            return {"type": "http.request", "body": b"{}", "more_body": False}
        return {"type": "http.disconnect"}
    req = Request({"type": "http", "method": "POST", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)}, receive=receive_empty)
    assert asyncio.run(slack(req)).status_code == 404
    req = Request({"type": "http", "method": "POST", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(telegram({}, req)).status_code == 404


def test_sessions_import_invalid_payload_and_broadcast(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    sessions_import = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/import")
    broadcast = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/{session_id}/broadcast")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(sessions_import({"sessions": "bad"}, req)).status_code == 400
    assert asyncio.run(broadcast("sess_a", {"message": ""}, req)).status_code == 400

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.connections["conn"] = DummyWS()
    server.STATE.conn_seq["conn"] = 0
    server.STATE.conn_sessions["conn"] = "sess_a"

    resp = asyncio.run(broadcast("sess_a", {"message": "hi"}, req))
    assert resp.status_code == 200
    assert sent
