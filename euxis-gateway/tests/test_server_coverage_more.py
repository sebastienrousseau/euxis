import asyncio
import json
import sys
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


def test_ensure_repo_paths(monkeypatch):
    repo_root = Path(server.__file__).resolve().parents[3]
    module_paths = [repo_root, repo_root / "api/src", repo_root / "packages/shared/src"]
    original_sys_path = list(sys.path)

    for path in map(str, module_paths):
        if path in sys.path:
            sys.path.remove(path)

    server._PATHS_INITIALIZED = False
    original_exists = Path.exists

    def fake_exists(self):
        if self in module_paths[1:]:
            return True
        return original_exists(self)

    monkeypatch.setattr(Path, "exists", fake_exists)
    server._ensure_repo_paths()
    assert str(repo_root) in sys.path
    assert str(repo_root / "api/src") in sys.path
    assert str(repo_root / "packages/shared/src") in sys.path

    server._ensure_repo_paths()

    server._PATHS_INITIALIZED = False
    server._ensure_repo_paths()

    sys.path[:] = original_sys_path


def test_security_headers_and_rate_limit(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["https_only"] = True
    _build_app(monkeypatch, config)
    assert config["gateway"]["https_only"] is True

    client_ip = "127.0.0.2"
    server._rate_limit_state.clear()
    for _ in range(server.RATE_LIMIT_MAX_REQUESTS):
        assert server._check_rate_limit(client_ip) is True
    assert server._check_rate_limit(client_ip) is False


def test_adapter_handler_and_startup_shutdown(monkeypatch):
    seen = {}
    tasks = []

    async def fake_process(session_id, content, meta, _config):
        seen["session_id"] = session_id
        seen["content"] = content
        seen["meta"] = meta

    def fake_build_adapters(_config, on_message):
        on_message("sess", "hello", {"channel_id": "slack", "chat_id": "c1", "sender": "alice"})
        return {"slack": SimpleNamespace(connect=lambda: seen.__setitem__("connect", True),
                                         disconnect=lambda: seen.__setitem__("disconnect", True))}

    def fake_create_task(coro):
        if getattr(coro, "cr_code", None) and coro.cr_code.co_name == "cron_loop":
            coro.close()
            return SimpleNamespace(cancel=lambda: None)
        tasks.append(coro)
        return SimpleNamespace(cancel=lambda: None)

    monkeypatch.setattr(server, "build_adapters", fake_build_adapters)
    monkeypatch.setattr(server, "process_inbound", fake_process)
    monkeypatch.setattr(server.asyncio, "create_task", fake_create_task)
    monkeypatch.setattr(server, "load_approvals", lambda: {})
    monkeypatch.setattr(server, "load_cron_jobs", lambda: [])
    monkeypatch.setattr(server, "load_webhooks", lambda: [])
    monkeypatch.setattr(server, "audit_log", lambda *_args, **_kwargs: None)

    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = server.build_app(config)

    async def run_lifespan():
        async with app.router.lifespan_context(app):
            pass

    asyncio.run(run_lifespan())

    for coro in tasks:
        asyncio.run(coro)

    assert seen["content"] == "hello"
    assert seen["meta"]["channel_id"] == "slack"
    assert seen.get("connect") is True
    assert seen.get("disconnect") is True


def test_startup_webhooks(monkeypatch):
    hooks = [{"url": "http://example.com", "events": ["agent.final"]}]
    monkeypatch.setattr(server, "load_approvals", lambda: {})
    monkeypatch.setattr(server, "load_cron_jobs", lambda: [])
    monkeypatch.setattr(server, "load_webhooks", lambda: hooks)
    monkeypatch.setattr(server, "build_adapters", lambda *_args, **_kwargs: {})
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = server.build_app(config)

    async def run_lifespan():
        async with app.router.lifespan_context(app):
            pass

    asyncio.run(run_lifespan())
    assert server.STATE.webhooks == hooks


def test_auth_guarded_endpoints(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    _build_app(monkeypatch, config)

    for path in ("/approvals", "/automation/cron", "/webhooks"):
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


def test_voice_upload_success(monkeypatch, tmp_path):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    monkeypatch.setattr(server, "persist_voice_blob", lambda *_args, **_kwargs: tmp_path / "audio.wav")
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
    data = b"RIFFxxxxWAVE" + b"0" * 4
    resp = asyncio.run(upload("sess", DummyFile("audio.wav", data), req))
    assert resp.status_code == 200
    assert json.loads(resp.body.decode("utf-8"))["status"] == "ok"


def test_voice_tts_and_stt_success(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "tts": {"mode": "webhook", "webhook_url": "http://example.com"}, "stt": {"mode": "command", "command": "echo hi"}}

    calls = []

    async def fake_post(url, payload):
        calls.append((url, payload))

    async def fake_run(_cmd, _vars):
        return "transcript"

    monkeypatch.setattr(server, "post_json", fake_post)
    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))
    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)
    app = _build_app(monkeypatch, config)
    tts_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/tts")
    stt_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/stt")

    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    tts_resp = asyncio.run(tts_endpoint({"session_id": "sess", "content": "hi"}, req))
    assert tts_resp.status_code == 200
    assert calls

    stt_resp = asyncio.run(stt_endpoint({"session_id": "sess", "audio_path": "file.wav"}, req))
    assert stt_resp.status_code == 200
    assert json.loads(stt_resp.body.decode("utf-8"))["content"] == "transcript"


def test_sessions_export_and_import(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    server.STATE.sessions["sess"] = [{"role": "user"}]
    app = _build_app(monkeypatch, config)
    export_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/export")
    import_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/import")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    exported = asyncio.run(export_endpoint(req))
    assert json.loads(exported.body.decode("utf-8"))["sessions"]["sess"][0]["role"] == "user"

    payload = {"sessions": {"sess": "bad", "sess2": [{"role": "assistant"}, "bad"]}}
    imported = asyncio.run(import_endpoint(payload, req))
    assert json.loads(imported.body.decode("utf-8"))["imported"] == 1


def test_session_detail_and_runs(monkeypatch, tmp_path):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    server.STATE.sessions["sess"] = [{"role": "user"}]
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"session_id": "sess"})
    app = _build_app(monkeypatch, config)
    (tmp_path / "run1.jsonl").write_text("{}\n", encoding="utf-8")
    detail_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/sessions/{session_id}")
    runs_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/runs")
    runs_endpoint.__globals__["runs_dir"] = lambda: tmp_path
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})

    detail = asyncio.run(detail_endpoint("sess", req))
    assert json.loads(detail.body.decode("utf-8"))["messages"][0]["role"] == "user"

    runs = asyncio.run(runs_endpoint(req))
    assert "run1" in json.loads(runs.body.decode("utf-8"))["runs"]


def test_approvals_dispatch(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)

    server.STATE.pending_approvals["run1"] = {
        "run_id": "run1",
        "session_id": "sess",
        "meta": {},
        "content": "hi",
    }

    class DummyWS:
        async def send_text(self, _text):
            return None

    server.STATE.connections["conn"] = DummyWS()
    server.STATE.conn_seq["conn"] = 0
    server.STATE.conn_sessions["conn"] = "sess"

    async def fake_dispatch(*args, **kwargs):
        server.STATE.running[args[2]] = asyncio.create_task(asyncio.sleep(0))
        return None

    monkeypatch.setattr(server, "dispatch_with_lock", fake_dispatch)
    approve_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/approvals/{run_id}/approve")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(approve_endpoint("run1", req))
    assert resp.status_code == 200
    assert "run1" in server.STATE.running


def test_approvals_dispatch_different_session(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)

    server.STATE.pending_approvals["run2"] = {
        "run_id": "run2",
        "session_id": "sess2",
        "meta": {},
        "content": "hi",
    }

    class DummyWS:
        async def send_text(self, _text):
            return None

    server.STATE.connections["conn2"] = DummyWS()
    server.STATE.conn_seq["conn2"] = 0
    server.STATE.conn_sessions["conn2"] = "other_sess"

    async def fake_dispatch(*_args, **_kwargs):
        raise RuntimeError("should not be called")

    monkeypatch.setattr(server, "dispatch_with_lock", fake_dispatch)
    approve_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/approvals/{run_id}/approve")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(approve_endpoint("run2", req))
    assert resp.status_code == 200
    assert "run2" not in server.STATE.running

def test_admin_exec_success(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    admin_exec_endpoint = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/admin/exec")
    req = Request({"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)})
    resp = asyncio.run(admin_exec_endpoint({"policy": "deny"}, req))
    assert resp.status_code == 200
    assert json.loads(resp.body.decode("utf-8"))["exec"]["policy"] == "deny"


def test_webchat_mount(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    webchat_dir = Path(server.__file__).resolve().parent / "webchat"
    original_exists = Path.exists

    def fake_exists(self):
        if self == webchat_dir:
            return True
        return original_exists(self)

    monkeypatch.setattr(Path, "exists", fake_exists)
    app = _build_app(monkeypatch, config)
    paths = [getattr(route, "path", "") for route in app.routes]
    assert "/webchat" in paths


def test_webchat_not_mounted(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    webchat_dir = Path(server.__file__).resolve().parent / "webchat"
    original_exists = Path.exists

    def fake_exists(self):
        if self == webchat_dir:
            return False
        return original_exists(self)

    monkeypatch.setattr(Path, "exists", fake_exists)
    app = _build_app(monkeypatch, config)
    paths = [getattr(route, "path", "") for route in app.routes]
    assert "/webchat" not in paths


def test_shutdown_cancels_cron(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)
    called = {"cancel": 0}
    dummy_task = SimpleNamespace(cancel=lambda: called.__setitem__("cancel", called["cancel"] + 1))

    def fake_create_task(coro):
        coro.close()
        return dummy_task

    monkeypatch.setattr(server.asyncio, "create_task", fake_create_task)

    async def run_lifespan():
        async with app.router.lifespan_context(app):
            pass

    asyncio.run(run_lifespan())
    assert called["cancel"] == 1


def test_shutdown_no_cron(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = _build_app(monkeypatch, config)

    def fake_create_task(coro):
        coro.close()
        return None

    monkeypatch.setattr(server.asyncio, "create_task", fake_create_task)

    async def run_lifespan():
        async with app.router.lifespan_context(app):
            pass

    asyncio.run(run_lifespan())
