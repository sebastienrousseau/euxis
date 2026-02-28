import pytest
import asyncio
import json
from types import SimpleNamespace

fastapi = pytest.importorskip("fastapi")
if not hasattr(fastapi, "__path__"):
    pytest.skip("fastapi test client unavailable in stubbed runtime", allow_module_level=True)

from starlette.requests import Request

from gateway import server


def build_test_app(monkeypatch, tmp_path):
    data = {
        "approvals": {},
        "cron": [],
        "webhooks": [],
        "canvas": {},
        "sessions": {},
        "runs": {},
    }

    monkeypatch.setattr(server, "load_approvals", lambda: data["approvals"])
    monkeypatch.setattr(server, "load_cron_jobs", lambda: data["cron"])
    monkeypatch.setattr(server, "load_webhooks", lambda: data["webhooks"])
    monkeypatch.setattr(server, "load_canvas_state", lambda session_id: data["canvas"].get(session_id, {}))
    monkeypatch.setattr(server, "persist_canvas_state", lambda session_id, state: data["canvas"].__setitem__(session_id, state))
    monkeypatch.setattr(server, "persist_cron_jobs", lambda jobs: data["cron"].__setitem__(slice(None), jobs))
    monkeypatch.setattr(server, "persist_webhooks", lambda hooks: data["webhooks"].__setitem__(slice(None), hooks))
    monkeypatch.setattr(server, "persist_approval", lambda run_id, entry: data["approvals"].__setitem__(run_id, entry))
    monkeypatch.setattr(server, "delete_approval", lambda run_id: data["approvals"].pop(run_id, None))
    monkeypatch.setattr(server, "audit_log", lambda _payload: None)
    monkeypatch.setattr(server, "load_session_from_disk", lambda session_id: data["sessions"].get(session_id, []))
    monkeypatch.setattr(server, "load_session_meta", lambda session_id: {"session_id": session_id})
    monkeypatch.setattr(server, "persist_session_meta", lambda session_id, meta: None)
    monkeypatch.setattr(
        server.asyncio,
        "create_task",
        lambda coro: (coro.close(), SimpleNamespace(cancel=lambda: None))[1],
    )
    def _persist_message(session_id, entry):
        data["sessions"].setdefault(session_id, []).append(entry)
        server.STATE.sessions.setdefault(session_id, []).append(entry)

    monkeypatch.setattr(server, "persist_message", _persist_message)
    monkeypatch.setattr(server, "load_run_events", lambda run_id: data["runs"].get(run_id, []))
    monkeypatch.setattr(server, "runs_dir", lambda: tmp_path)

    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["canvas"] = {"enabled": True}
    config["gateway"]["voice"] = {"enabled": False}
    app = server.build_app(config)
    return app, data


def test_health_and_sessions(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    health = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/health")
    sessions = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/sessions" and "GET" in (getattr(route, "methods", set()) or set())
    )
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(health(req))
    assert resp.status_code == 200
    sessions_resp = asyncio.run(sessions(req))
    assert sessions_resp.status_code == 200
    assert json.loads(sessions_resp.body.decode("utf-8"))["sessions"] == []


def test_canvas_endpoints(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    canvas_set = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/canvas/{session_id}" and "POST" in (getattr(route, "methods", set()) or set())
    )
    canvas_get = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/canvas/{session_id}" and "GET" in (getattr(route, "methods", set()) or set())
    )
    canvas_validate = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/canvas/{session_id}/validate" and "POST" in (getattr(route, "methods", set()) or set())
    )
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(canvas_set("sess_a", {"state": {"widgets": []}}, req)).status_code == 200
    fetched = asyncio.run(canvas_get("sess_a", req))
    assert json.loads(fetched.body.decode("utf-8"))["state"]["widgets"] == []
    validate = asyncio.run(canvas_validate("sess_a", {"state": {"widgets": []}}, req))
    assert json.loads(validate.body.decode("utf-8"))["status"] == "ok"


def test_cron_and_webhooks(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    cron_create = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/automation/cron" and "POST" in (getattr(route, "methods", set()) or set())
    )
    cron_list = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/automation/cron" and "GET" in (getattr(route, "methods", set()) or set())
    )
    cron_delete = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/automation/cron/{job_id}" and "DELETE" in (getattr(route, "methods", set()) or set())
    )
    hooks_set = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/webhooks" and "POST" in (getattr(route, "methods", set()) or set())
    )
    hooks_list = next(
        route.endpoint
        for route in app.routes
        if getattr(route, "path", None) == "/webhooks" and "GET" in (getattr(route, "methods", set()) or set())
    )
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    assert asyncio.run(cron_create({"job_id": "job", "every_seconds": 60, "session_id": "sess", "content": "ping"}, req)).status_code == 200
    jobs = asyncio.run(cron_list(req))
    assert len(json.loads(jobs.body.decode("utf-8"))["jobs"]) == 1
    assert asyncio.run(cron_delete("job", req)).status_code == 200
    assert asyncio.run(hooks_set({"webhooks": [{"url": "https://example.com", "events": ["agent.final"]}]}, req)).status_code == 200
    listed = asyncio.run(hooks_list(req))
    assert json.loads(listed.body.decode("utf-8"))["webhooks"][0]["url"].startswith("https://")


def test_sessions_import_export(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    sessions_import = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/import")
    sessions_export = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/export")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    payload = {"sessions": {"sess_a": [{"role": "user", "content": "hi"}]}}
    assert asyncio.run(sessions_import(payload, req)).status_code == 200
    exported = asyncio.run(sessions_export(req))
    assert exported.status_code == 200
    exported_json = json.loads(exported.body.decode("utf-8"))
    assert "sessions" in exported_json
    assert "sess_a" in exported_json["sessions"]


def test_approvals_flow(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    approvals = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/approvals")
    approve = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/approvals/{run_id}/approve")
    reject = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/approvals/{run_id}/reject")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    data["approvals"]["run1"] = {"run_id": "run1"}
    pending = asyncio.run(approvals(req))
    assert json.loads(pending.body.decode("utf-8"))["pending"][0]["run_id"] == "run1"
    assert asyncio.run(approve("run1", req)).status_code == 200
    data["approvals"]["run2"] = {"run_id": "run2"}
    assert asyncio.run(reject("run2", req)).status_code == 200


def test_session_detail_and_runs(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    detail = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/{session_id}")
    runs = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/runs")
    run_detail = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/runs/{run_id}")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    data["sessions"]["sess_a"] = [{"role": "user"}]
    assert asyncio.run(detail("sess_a", req)).status_code == 200
    server.STATE.running.clear()
    runs_resp = asyncio.run(runs(req))
    assert json.loads(runs_resp.body.decode("utf-8"))["runs"] == []
    run_detail_resp = asyncio.run(run_detail("run_1", req))
    assert json.loads(run_detail_resp.body.decode("utf-8"))["events"] == []
