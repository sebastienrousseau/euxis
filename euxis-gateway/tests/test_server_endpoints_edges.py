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
from gateway.routers import mesh as mesh_router


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
    monkeypatch.setattr(
        server.asyncio,
        "create_task",
        lambda coro: (coro.close(), SimpleNamespace(cancel=lambda: None))[1],
    )
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
    _build_app(monkeypatch, config)

    for path in (
        "/automation/cron",
        "/automation/cron/job",
        "/webhooks",
        "/sessions",
        "/canvas/sess",
        "/canvas/sess/validate",
        "/voice/wake",
        "/voice/talk",
        "/voice/tts",
        "/voice/stt",
        "/sessions/sess/broadcast",
        "/sessions/export",
        "/sessions/sess",
        "/runs",
        "/runs/run1",
        "/approvals/run1/approve",
        "/approvals/run1/reject",
        "/admin/exec",
    ):
        req = Request(
            {
                "type": "http",
                "method": "GET",
                "path": path,
                "headers": [],
                "query_string": b"",
                "client": ("127.0.0.2", 1234),
            }
        )
        allowed, reason = server.is_http_authorized(req, config)
        assert allowed is False
        assert reason == "invalid_token"


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
    slack_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/slack/events")
    telegram_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/telegram/webhook")

    body = json.dumps({"type": "event_callback"}).encode("utf-8")
    ts = str(int(server.time.time()))
    base = f"v0:{ts}:".encode("utf-8") + body
    digest = server.hmac.new(b"secret", base, server.hashlib.sha256).hexdigest()
    signature = f"v0={digest}"

    received_once = False

    async def receive():
        nonlocal received_once
        if not received_once:
            received_once = True
            return {"type": "http.request", "body": body, "more_body": False}
        return {"type": "http.disconnect"}

    slack_req = Request(
        {
            "type": "http",
            "method": "POST",
            "path": "/channels/slack/events",
            "headers": [
                (b"x-slack-request-timestamp", ts.encode("utf-8")),
                (b"x-slack-signature", signature.encode("utf-8")),
            ],
            "query_string": b"",
            "client": ("127.0.0.2", 1234),
        },
        receive=receive,
    )
    slack_resp = asyncio.run(slack_endpoint(slack_req))
    assert slack_resp.status_code == 200

    telegram_req = Request(
        {
            "type": "http",
            "method": "POST",
            "path": "/channels/telegram/webhook",
            "headers": [(b"x-telegram-bot-api-secret-token", b"secret")],
            "query_string": b"",
            "client": ("127.0.0.2", 1234),
        }
    )
    telegram_resp = asyncio.run(telegram_endpoint({"update_id": 1}, telegram_req))
    assert telegram_resp.status_code == 200
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
    endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/channels/telegram/webhook")
    req = Request(
        {
            "type": "http",
            "method": "POST",
            "headers": [(b"x-telegram-bot-api-secret-token", b"secret")],
            "query_string": b"",
            "client": ("127.0.0.2", 1234),
        }
    )
    resp = asyncio.run(endpoint({"update_id": 3}, req))
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

    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "process_inbound", lambda *_args, **_kwargs: asyncio.sleep(0))
    wake = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/wake")
    talk = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/talk")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    assert asyncio.run(wake({"session_id": "s", "content": "wake"}, req)).status_code == 200
    assert asyncio.run(talk({"session_id": "s", "content": "talk"}, req)).status_code == 200


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
    tts = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/tts")
    stt = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/stt")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(tts({"session_id": "s", "content": "x"}, req)).status_code == 404
    assert asyncio.run(stt({"session_id": "s"}, req)).status_code == 404


def test_voice_stt_invalid_session(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "command", "command": "echo hi"}}
    app = _build_app(monkeypatch, config)
    stt = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/stt")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(stt({"session_id": ""}, req)).status_code == 400


def test_voice_tts_webhook(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "tts": {"mode": "webhook", "webhook_url": "http://example.com"}}

    calls = []

    async def fake_post(url, payload):
        calls.append((url, payload))

    monkeypatch.setattr(server, "post_json", fake_post)
    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))
    app = _build_app(monkeypatch, config)
    tts = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/tts")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    resp = asyncio.run(tts({"session_id": "s", "content": "hello"}, req))
    assert resp.status_code == 200
    assert calls


def test_voice_stt_transcript(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "command", "command": "echo hi"}}

    async def fake_run(*_args, **_kwargs):
        return "transcript"

    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)
    app = _build_app(monkeypatch, config)
    stt = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/stt")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    resp = asyncio.run(stt({"session_id": "s", "audio_path": "file.wav"}, req))
    assert json.loads(resp.body.decode("utf-8"))["content"] == "transcript"


def test_voice_tts_no_webhook(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    tts = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/tts")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    async def boom(*_args, **_kwargs):
        raise RuntimeError("should not post")

    monkeypatch.setattr(server, "post_json", boom)
    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))

    resp = asyncio.run(tts({"session_id": "s", "content": "hello"}, req))
    assert resp.status_code == 200


def test_voice_stt_no_transcript(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "stt": {"mode": "command", "command": "echo hi"}}
    app = _build_app(monkeypatch, config)
    stt = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/stt")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    async def fake_run(*_args, **_kwargs):
        return ""

    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: (_ for _ in ()).throw(RuntimeError("no persist")))

    resp = asyncio.run(stt({"session_id": "s", "audio_path": "file.wav"}, req))
    assert json.loads(resp.body.decode("utf-8"))["content"] == ""


def test_voice_tts_with_session(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = _build_app(monkeypatch, config)
    tts = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/tts")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))
    resp = asyncio.run(tts({"session_id": "s", "content": "hello"}, req))
    assert resp.status_code == 200


def test_session_broadcast_continue(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    broadcast = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/{session_id}/broadcast")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    class DummyWS:
        async def send_text(self, _text):
            return None

    server.STATE.connections["conn"] = DummyWS()
    server.STATE.conn_sessions["conn"] = "other"
    server.STATE.conn_seq["conn"] = 0

    resp = asyncio.run(broadcast("sess", {"message": "hi"}, req))
    assert resp.status_code == 200


def test_sessions_list(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    server.STATE.sessions["sess"] = [{"role": "user"}]
    app = _build_app(monkeypatch, config)
    sessions = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(sessions(req))
    assert json.loads(resp.body.decode("utf-8"))["sessions"][0]["message_count"] == 1


def test_canvas_disabled_post(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["canvas"] = {"enabled": False}
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
    assert asyncio.run(canvas_set("sess", {"state": {}}, req)).status_code == 404
    assert asyncio.run(canvas_validate("sess", {"state": {}}, req)).status_code == 404


def test_session_detail_loads_disk(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    server.STATE.sessions["sess"] = []
    app = _build_app(monkeypatch, config)
    monkeypatch.setattr(mesh_router, "load_session_from_disk", lambda _sid: [{"role": "user"}])
    detail = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/{session_id}")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(detail("sess", req))
    assert json.loads(resp.body.decode("utf-8"))["messages"][0]["role"] == "user"


def test_session_detail_no_reload(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    server.STATE.sessions["sess"] = [{"role": "assistant"}]
    monkeypatch.setattr(server, "load_session_from_disk", lambda _sid: (_ for _ in ()).throw(RuntimeError("no reload")))
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"session_id": "sess"})
    app = _build_app(monkeypatch, config)
    detail = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/{session_id}")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(detail("sess", req))
    assert json.loads(resp.body.decode("utf-8"))["messages"][0]["role"] == "assistant"


def test_approvals_not_found(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    approve = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/approvals/{run_id}/approve")
    reject = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/approvals/{run_id}/reject")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(approve("missing", req)).status_code == 404
    assert asyncio.run(reject("missing", req)).status_code == 404


def test_admin_exec_with_token(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["admin"] = {"token": "admin"}
    app = _build_app(monkeypatch, config)
    admin_exec = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/admin/exec")
    req = Request(
        {
            "type": "http",
            "headers": [(b"x-admin-token", b"admin")],
            "query_string": b"",
            "client": ("127.0.0.2", 1234),
        }
    )
    resp = asyncio.run(admin_exec({"policy": "deny"}, req))
    assert resp.status_code == 200
