import asyncio
import json

import pytest

fastapi = pytest.importorskip("fastapi")
if hasattr(fastapi, "__path__"):
    pytest.skip("stub-only tests; real FastAPI present", allow_module_level=True)

from gateway import server


def _get_route(app, method, path):
    for meth, route_path, fn in app.routes:
        if meth == method and route_path == path:
            return fn
    raise AssertionError(f"Route {method} {path} not found")


def _request(headers=None, query=None):
    headers = headers or {}
    query = query or {}
    return server.Request(headers=headers, query_params=query, client=type("C", (), {"host": "127.0.0.1"})())


def test_build_app_routes_and_health(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["canvas"] = {"enabled": True}
    config["gateway"]["voice"] = {"enabled": False}
    monkeypatch.setattr(server, "load_approvals", lambda: {})
    monkeypatch.setattr(server, "load_cron_jobs", lambda: [])
    monkeypatch.setattr(server, "load_webhooks", lambda: [])
    monkeypatch.setattr(server, "build_adapters", lambda *_args, **_kwargs: {})

    app = server.build_app(config)
    health = _get_route(app, "GET", "/health")
    resp = asyncio.run(health(_request()))
    assert resp.status_code == 200


def test_sessions_import_export_stub(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = server.build_app(config)

    monkeypatch.setattr(server, "persist_message", lambda session_id, entry: None)
    monkeypatch.setattr(server, "load_session_from_disk", lambda session_id: [{"role": "user"}])

    import_fn = _get_route(app, "POST", "/sessions/import")
    export_fn = _get_route(app, "GET", "/sessions/export")

    payload = {"sessions": {"sess_a": [{"role": "user", "content": "hi"}]}}
    resp = asyncio.run(import_fn(payload, _request()))
    assert resp.status_code == 200

    server.STATE.sessions["sess_a"] = []
    export = asyncio.run(export_fn(_request()))
    assert "sess_a" in export.json()["sessions"]


def test_canvas_stub(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["canvas"] = {"enabled": True}
    app = server.build_app(config)

    monkeypatch.setattr(server, "load_canvas_state", lambda session_id: {"widgets": []})
    monkeypatch.setattr(server, "persist_canvas_state", lambda session_id, state: None)

    get_fn = _get_route(app, "GET", "/canvas/{session_id}")
    set_fn = _get_route(app, "POST", "/canvas/{session_id}")

    result = asyncio.run(get_fn("sess", _request()))
    assert result.json()["state"]["widgets"] == []
    set_resp = asyncio.run(set_fn("sess", {"state": {}}, _request()))
    assert set_resp.status_code == 200


def test_voice_upload_disabled(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": False}
    app = server.build_app(config)

    upload = _get_route(app, "POST", "/voice/upload")
    resp = asyncio.run(upload("sess", server.UploadFile("audio.wav", b"data"), _request()))
    assert resp.status_code == 404


def test_session_broadcast(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = server.build_app(config)

    class DummyWS:
        def __init__(self):
            self.sent = []

        async def send_text(self, payload):
            self.sent.append(json.loads(payload))

    server.STATE.connections = {"c1": DummyWS()}
    server.STATE.conn_sessions = {"c1": "sess"}
    server.STATE.conn_seq = {"c1": 0}

    broadcast = _get_route(app, "POST", "/sessions/{session_id}/broadcast")
    resp = asyncio.run(broadcast("sess", {"message": "hi"}, _request()))
    assert resp.status_code == 200
