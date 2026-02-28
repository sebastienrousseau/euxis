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


def _build_app(
    monkeypatch,
    config,
    adapters=None,
    load_session_from_disk_fn=None,
    dispatch_with_lock_fn=None,
):
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
    if load_session_from_disk_fn is None:
        load_session_from_disk_fn = lambda _sid: []
    monkeypatch.setattr(server, "load_session_from_disk", load_session_from_disk_fn)
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {})
    monkeypatch.setattr(server, "persist_session_meta", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_message", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "load_run_events", lambda _rid: [])
    monkeypatch.setattr(server, "run_voice_command", lambda *_args, **_kwargs: "")
    if dispatch_with_lock_fn is not None:
        monkeypatch.setattr(server, "dispatch_with_lock", dispatch_with_lock_fn)
    return server.build_app(config)


def _route(app, path: str, method: str = "GET"):
    for route in app.routes:
        if getattr(route, "path", None) == path and method in (getattr(route, "methods", set()) or set()):
            return route.endpoint
    raise AssertionError(f"route not found: {method} {path}")


def _http_request():
    return Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})


async def _asgi_get(app, path: str):
    sent = []
    emitted = {"done": False}
    scope = {
        "type": "http",
        "asgi": {"version": "3.0"},
        "http_version": "1.1",
        "method": "GET",
        "scheme": "http",
        "path": path,
        "raw_path": path.encode("utf-8"),
        "query_string": b"",
        "headers": [],
        "client": ("127.0.0.2", 1234),
        "server": ("testserver", 80),
    }

    async def receive():
        if emitted["done"]:
            return {"type": "http.disconnect"}
        emitted["done"] = True
        return {"type": "http.request", "body": b"", "more_body": False}

    async def send(message):
        sent.append(message)

    await app(scope, receive, send)
    return sent


def test_mesh_and_orchestrator_unauthorized_branches(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    app = _build_app(monkeypatch, config)
    req = _http_request()

    assert asyncio.run(_route(app, "/sessions", "GET")(req)).status_code == 401
    assert asyncio.run(_route(app, "/sessions/export", "GET")(req)).status_code == 401
    assert asyncio.run(_route(app, "/sessions/import", "POST")({"sessions": {}}, req)).status_code == 401
    assert asyncio.run(_route(app, "/sessions/{session_id}", "GET")("s1", req)).status_code == 401
    assert asyncio.run(_route(app, "/canvas/{session_id}", "GET")("s1", req)).status_code == 401
    assert asyncio.run(_route(app, "/canvas/{session_id}", "POST")("s1", {"state": {}}, req)).status_code == 401
    assert asyncio.run(_route(app, "/canvas/{session_id}/validate", "POST")("s1", {"state": {}}, req)).status_code == 401
    assert asyncio.run(_route(app, "/voice/wake", "POST")({"session_id": "s", "content": "x"}, req)).status_code == 401
    assert asyncio.run(_route(app, "/voice/talk", "POST")({"session_id": "s", "content": "x"}, req)).status_code == 401
    assert asyncio.run(_route(app, "/voice/tts", "POST")({"session_id": "s", "content": "x"}, req)).status_code == 401
    assert asyncio.run(_route(app, "/voice/stt", "POST")({"session_id": "s", "audio_path": "a.wav"}, req)).status_code == 401
    assert asyncio.run(_route(app, "/sessions/{session_id}/broadcast", "POST")("s1", {"message": "x"}, req)).status_code == 401

    assert asyncio.run(_route(app, "/approvals", "GET")(req)).status_code == 401
    assert asyncio.run(_route(app, "/automation/cron", "GET")(req)).status_code == 401
    assert asyncio.run(_route(app, "/automation/cron", "POST")({}, req)).status_code == 401
    assert asyncio.run(_route(app, "/automation/cron/{job_id}", "DELETE")("job1", req)).status_code == 401
    assert asyncio.run(_route(app, "/runs", "GET")(req)).status_code == 401
    assert asyncio.run(_route(app, "/runs/{run_id}", "GET")("run1", req)).status_code == 401
    assert asyncio.run(_route(app, "/approvals/{run_id}/approve", "POST")("run1", req)).status_code == 401
    assert asyncio.run(_route(app, "/approvals/{run_id}/reject", "POST")("run1", req)).status_code == 401


def test_sessions_export_loads_disk_when_session_empty(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(
        monkeypatch,
        config,
        load_session_from_disk_fn=lambda _sid: [{"role": "user", "content": "hi"}],
    )
    server.STATE.sessions["s1"] = []
    req = _http_request()
    resp = asyncio.run(_route(app, "/sessions/export", "GET")(req))
    payload = json.loads(resp.body.decode("utf-8"))
    assert payload["sessions"]["s1"][0]["content"] == "hi"


def test_orchestrator_approve_dispatch_branch(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"

    calls = []

    async def fake_dispatch(*args, **kwargs):
        calls.append((args, kwargs))

    app = _build_app(monkeypatch, config, dispatch_with_lock_fn=fake_dispatch)
    req = _http_request()

    server.STATE.pending_approvals["run1"] = {
        "run_id": "run1",
        "session_id": "s1",
        "meta": {},
        "content": "hello",
        "approved": False,
    }
    server.STATE.connections["c1"] = object()
    server.STATE.conn_sessions["c1"] = "s1"
    server.STATE.conn_seq["c1"] = 2

    resp = asyncio.run(_route(app, "/approvals/{run_id}/approve", "POST")("run1", req))
    assert resp.status_code == 200
    assert calls
    assert "run1" in server.STATE.running


def test_middleware_rate_limit_and_hsts(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["https_only"] = True
    app = _build_app(monkeypatch, config)

    monkeypatch.setattr(server, "_check_rate_limit", lambda _ip: False)
    blocked = asyncio.run(_asgi_get(app, "/health"))
    start = next(msg for msg in blocked if msg["type"] == "http.response.start")
    blocked_headers = {k.decode("utf-8"): v.decode("utf-8") for k, v in start["headers"]}
    assert start["status"] == 429
    assert blocked_headers["retry-after"] == str(server.RATE_LIMIT_WINDOW)

    monkeypatch.setattr(server, "_check_rate_limit", lambda _ip: True)
    ok = asyncio.run(_asgi_get(app, "/health"))
    start = next(msg for msg in ok if msg["type"] == "http.response.start")
    ok_headers = {k.decode("utf-8"): v.decode("utf-8") for k, v in start["headers"]}
    assert start["status"] == 200
    assert ok_headers["strict-transport-security"] == "max-age=31536000; includeSubDomains"

    # Also cover the non-HTTPS branch where HSTS must be absent.
    config["gateway"]["https_only"] = False
    app = _build_app(monkeypatch, config)
    ok = asyncio.run(_asgi_get(app, "/health"))
    start = next(msg for msg in ok if msg["type"] == "http.response.start")
    no_hsts_headers = {k.decode("utf-8"): v.decode("utf-8") for k, v in start["headers"]}
    assert start["status"] == 200
    assert "strict-transport-security" not in no_hsts_headers


def test_mcp_authorized_host_connection(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)

    class FakeHost:
        def __init__(self):
            self.called = False

        async def handle_connection(self, ws):
            self.called = True

    class FakeWS:
        headers = {}
        query_params = {}

    host = FakeHost()
    server.STATE.mcp_host = host
    endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/mcp")
    asyncio.run(endpoint(FakeWS()))
    assert host.called is True

    # Authorized connection when MCP host is absent should no-op safely.
    server.STATE.mcp_host = None
    asyncio.run(endpoint(FakeWS()))


def test_handle_frame_event_broadcast_swallow_send_error():
    cfg = server.load_config(None)

    class Sender:
        async def send_text(self, _payload):
            return None

    class FailingWS:
        async def send_text(self, _payload):
            raise RuntimeError("send failed")

    sender = Sender()
    server.STATE.connections = {"fail": FailingWS(), "sender": sender}
    raw = json.dumps({"type": "event", "event": "node.browser.stream_frame", "data": {"frame": "x"}})
    asyncio.run(server.handle_frame(sender, raw, {"value": 0}, cfg, "conn1"))


def test_is_exec_allowed_and_process_inbound_unconnected_branches(monkeypatch):
    cfg = server.load_config(None)
    cfg["gateway"]["exec"]["policy"] = "allowlist"
    cfg["gateway"]["exec"]["ask_fallback"] = "deny"
    cfg["gateway"]["exec"]["allowlist"] = []

    # Force non-standard trust to bypass restricted/trusted short-circuits.
    monkeypatch.setattr(
        server.STATE.identity_manager,
        "get_identity",
        lambda _agent: SimpleNamespace(trust_level="custom"),
    )
    cfg["gateway"]["exec"]["ask"] = "always"
    assert server.is_exec_allowed({"agent": "unknown", "approved": False}, cfg) is False
    cfg["gateway"]["exec"]["ask"] = "on-miss"
    assert server.is_exec_allowed({"agent": "unknown", "approved": False}, cfg) is False

    monkeypatch.setattr(server, "persist_session_meta", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_message", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {})
    server.STATE.connections = {}
    asyncio.run(server.process_inbound("s1", "hello", {"agent": "architect", "approved": True}, cfg))
    assert not server.STATE.running
