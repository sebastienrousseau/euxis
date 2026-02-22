import pytest

fastapi = pytest.importorskip("fastapi")
if not hasattr(fastapi, "__path__"):
    pytest.skip("fastapi test client unavailable in stubbed runtime", allow_module_level=True)

from fastapi.testclient import TestClient

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
    client = TestClient(app)
    resp = client.get("/health")
    assert resp.status_code == 200
    sessions = client.get("/sessions")
    assert sessions.status_code == 200
    assert sessions.json()["sessions"] == []


def test_canvas_endpoints(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    client = TestClient(app)
    resp = client.post("/canvas/sess_a", json={"state": {"widgets": []}})
    assert resp.status_code == 200
    fetched = client.get("/canvas/sess_a")
    assert fetched.json()["state"]["widgets"] == []
    validate = client.post("/canvas/sess_a/validate", json={"state": {"widgets": []}})
    assert validate.json()["status"] == "ok"


def test_cron_and_webhooks(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    client = TestClient(app)
    cron = client.post("/automation/cron", json={"job_id": "job", "every_seconds": 60, "session_id": "sess", "content": "ping"})
    assert cron.status_code == 200
    cron_list = client.get("/automation/cron")
    assert len(cron_list.json()["jobs"]) == 1
    delete = client.delete("/automation/cron/job")
    assert delete.status_code == 200

    hooks = client.post("/webhooks", json={"webhooks": [{"url": "https://example.com", "events": ["agent.final"]}]})
    assert hooks.status_code == 200
    listed = client.get("/webhooks")
    assert listed.json()["webhooks"][0]["url"].startswith("https://")


def test_sessions_import_export(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    client = TestClient(app)
    payload = {"sessions": {"sess_a": [{"role": "user", "content": "hi"}]}}
    resp = client.post("/sessions/import", json=payload)
    assert resp.status_code == 200
    exported = client.get("/sessions/export")
    assert "sess_a" in exported.json()["sessions"]


def test_approvals_flow(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    client = TestClient(app)
    data["approvals"]["run1"] = {"run_id": "run1"}
    list_resp = client.get("/approvals")
    assert list_resp.json()["pending"][0]["run_id"] == "run1"
    approve = client.post("/approvals/run1/approve")
    assert approve.status_code == 200
    data["approvals"]["run2"] = {"run_id": "run2"}
    reject = client.post("/approvals/run2/reject")
    assert reject.status_code == 200


def test_session_detail_and_runs(monkeypatch, tmp_path):
    app, data = build_test_app(monkeypatch, tmp_path)
    client = TestClient(app)
    data["sessions"]["sess_a"] = [{"role": "user"}]
    detail = client.get("/sessions/sess_a")
    assert detail.status_code == 200
    server.STATE.running.clear()
    runs = client.get("/runs")
    assert runs.json()["runs"] == []
    run_detail = client.get("/runs/run_1")
    assert run_detail.json()["events"] == []
