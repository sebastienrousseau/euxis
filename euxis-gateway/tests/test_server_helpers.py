import asyncio
import sys
import base64
import hmac
import json
from types import SimpleNamespace
from pathlib import Path

import pytest

pytest.importorskip("fastapi")

from gateway import server


@pytest.fixture(autouse=True)
def reset_state(monkeypatch):
    server.STATE = server.GatewayState()
    server._rate_limit_state.clear()
    server._auth_failure_state.clear()
    yield


def test_merge_dicts_nested():
    base = {"a": {"b": 1}, "c": 2}
    override = {"a": {"b": 3}, "d": 4}
    merged = server.merge_dicts(base, override)
    assert merged["a"]["b"] == 3
    assert merged["c"] == 2
    assert merged["d"] == 4


def test_load_config_missing_path(tmp_path, capsys):
    missing = tmp_path / "missing.json"
    config = server.load_config(missing)
    assert config["gateway"]["port"] == server.DEFAULT_CONFIG["gateway"]["port"]
    err = capsys.readouterr().err
    assert "not found" in err


def test_load_config_invalid_json(tmp_path, capsys):
    bad = tmp_path / "bad.json"
    bad.write_text("not-json", encoding="utf-8")
    config = server.load_config(bad)
    assert config["gateway"]["port"] == server.DEFAULT_CONFIG["gateway"]["port"]
    assert "Failed to read config" in capsys.readouterr().err


def test_load_config_non_object(tmp_path, capsys):
    bad = tmp_path / "bad.json"
    bad.write_text("[]", encoding="utf-8")
    config = server.load_config(bad)
    assert config["gateway"]["port"] == server.DEFAULT_CONFIG["gateway"]["port"]
    assert "Gateway config must be" in capsys.readouterr().err


def test_resolve_config_from_args(monkeypatch, tmp_path):
    cfg = tmp_path / "cfg.json"
    cfg.write_text(json.dumps({"gateway": {"port": 1234}}), encoding="utf-8")
    args = SimpleNamespace(config=str(cfg), bind="0.0.0.0", port=9999, auth_mode="none")
    config = server.resolve_config(args)
    assert config["gateway"]["bind"] == "0.0.0.0"
    assert config["gateway"]["port"] == 9999
    assert config["gateway"]["auth"]["mode"] == "none"


def test_os_environ_copy(monkeypatch):
    monkeypatch.setenv("EUXIS_TEST", "1")
    env = server.os_environ()
    assert env["EUXIS_TEST"] == "1"


def test_verify_slack_signature_valid():
    body = b"payload"
    ts = str(int(server.time.time()))
    base = f"v0:{ts}:".encode("utf-8") + body
    secret = "secret"
    digest = hmac.new(secret.encode("utf-8"), base, server.hashlib.sha256).hexdigest()
    signature = f"v0={digest}"
    request = SimpleNamespace(headers={"X-Slack-Request-Timestamp": ts, "X-Slack-Signature": signature})
    assert server.verify_slack_signature(request, body, secret) is True


def test_verify_slack_signature_invalid_timestamp():
    request = SimpleNamespace(headers={"X-Slack-Request-Timestamp": "bad", "X-Slack-Signature": "sig"})
    assert server.verify_slack_signature(request, b"", "secret") is False


def test_validate_audio_magic():
    blob = b"RIFFxxxxWAVE" + b"0" * 4
    assert server._validate_audio_magic(blob, "wav") is True
    assert server._validate_audio_magic(b"short", "wav") is False
    assert server._validate_audio_magic(blob, "unknown") is False


def test_rate_limit_and_lockout():
    client = "1.2.3.4"
    server._rate_limit_state[client] = [server.time.time()] * server.RATE_LIMIT_MAX_REQUESTS
    assert server._check_rate_limit(client) is False
    assert server._check_auth_lockout(client) is True
    for _ in range(server.RATE_LIMIT_AUTH_FAILURES):
        server._record_auth_failure(client)
    assert server._check_auth_lockout(client) is False


class DummyRequest:
    def __init__(self, headers: dict, query_string: str = "") -> None:
        self.headers = {k.lower(): v for k, v in headers.items()}
        self.query_params = {}
        if query_string:
            for part in query_string.split("&"):
                if not part:
                    continue
                key, value = part.split("=", 1)
                self.query_params[key] = value
        self.client = SimpleNamespace(host="127.0.0.2")


def _request_with_headers(headers: dict, query: str = ""):
    return DummyRequest(headers, query)


def test_is_http_authorized_token(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    req = _request_with_headers({"authorization": "Bearer tok"})
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is True
    assert reason is None


def test_is_http_authorized_query_param(monkeypatch, capsys):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    req = _request_with_headers({}, query="token=tok")
    allowed, _ = server.is_http_authorized(req, config)
    assert allowed is True
    assert events
    assert "auth_query_param_used" in events[0]["event"]
    assert "SECURITY WARNING" in capsys.readouterr().err


def test_is_authorized_basic_password():
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "password"
    config["gateway"]["auth"]["password"]["value"] = "pw"
    creds = base64.b64encode(b"user:pw").decode("utf-8")
    ws = SimpleNamespace(headers={"authorization": f"Basic {creds}"}, query_params={}, client=None)
    allowed, _ = server.is_authorized(ws, config)
    assert allowed is True


def test_is_authorized_none_mode():
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    ws = SimpleNamespace(headers={}, query_params={}, client=None)
    allowed, _ = server.is_authorized(ws, config)
    assert allowed is True


def test_is_exec_allowed_variants():
    cfg = server.load_config(None)
    cfg["gateway"]["exec"]["policy"] = "deny"
    assert server.is_exec_allowed({}, cfg) is False
    cfg["gateway"]["exec"]["policy"] = "full"
    assert server.is_exec_allowed({}, cfg) is True
    cfg["gateway"]["exec"]["policy"] = "allowlist"
    cfg["gateway"]["exec"]["allowlist"] = ["agent"]
    assert server.is_exec_allowed({"agent": "agent"}, cfg) is True
    cfg["gateway"]["exec"]["ask"] = "off"
    cfg["gateway"]["exec"]["ask_fallback"] = "full"
    assert server.is_exec_allowed({"agent": "nope"}, cfg) is True


def test_is_message_allowed_variants():
    cfg = server.load_config(None)
    assert server.is_message_allowed({"scope": "dm"}, cfg) is True
    cfg["gateway"]["policy"]["allowlist"] = ["alice"]
    assert server.is_message_allowed({"scope": "channel", "sender": "bob"}, cfg) is False
    assert server.is_message_allowed({"scope": "channel", "sender": "alice", "mentions_bot": False}, cfg) is False
    assert server.is_message_allowed({"scope": "channel", "sender": "alice", "mentions_bot": True}, cfg) is True


def test_validate_canvas_errors():
    errors = server.validate_canvas({"widgets": "bad"})
    assert "widgets must be a list" in errors
    errors = server.validate_canvas({"widgets": ["bad", {}]})
    assert "widget[0] must be an object" in errors
    assert "widget[1] missing type" in errors
    errors = server.validate_canvas({"widgets": [{"type": "chart"}, {}]})
    assert "widget[1] missing type" in errors


def test_split_message():
    assert server.split_message("abc", 0) == ["abc"]
    assert server.split_message("abc", 5) == ["abc"]
    assert server.split_message("abcd", 2) == ["ab", "cd"]


def test_sanitize_voice_vars():
    sanitized = server._sanitize_voice_vars({"text": "hello; rm -rf /"})
    assert sanitized["text"].startswith("'")
    assert sanitized["text"].endswith("'")


def test_run_voice_command(monkeypatch):
    server.STATE.config = {"gateway": {"voice": {"command_allowlist": ["echo"]}}}

    class DummyProc:
        returncode = 0

        async def communicate(self):
            return b"/tmp/out.wav", b""

    async def fake_exec(*_args, **_kwargs):
        return DummyProc()

    monkeypatch.setattr(asyncio, "create_subprocess_exec", fake_exec)
    output = asyncio.run(server.run_voice_command("echo {text}", {"text": "hi"}))
    assert output == "/tmp/out.wav"


def test_send_result_and_error():
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    ws = DummyWS()
    asyncio.run(server.send_result(ws, "1", {"ok": True}))
    asyncio.run(server.send_error(ws, "2", "ERR", "bad"))
    assert sent[0]["ok"] is True
    assert sent[1]["ok"] is False

def test_cmd_helpers(monkeypatch, capsys):
    args = type("Args", (), {"bind": "127.0.0.2", "port": 10000, "config": None, "auth_mode": None})()
    monkeypatch.setattr(server, "resolve_config", lambda _args: server.load_config(None))
    monkeypatch.setattr(server.uvicorn, "run", lambda *_args, **_kwargs: None)
    assert server.cmd_run(args) == 0

    monkeypatch.setattr(server, "resolve_config", lambda _args: {"gateway": {"bind": "127.0.0.2", "port": 1, "health_path": "/health", "channels": {}}})

    class DummyResp:
        status_code = 200
        text = "ok"

    httpx_stub = type("httpx", (), {"get": lambda *_args, **_kwargs: DummyResp()})
    monkeypatch.setitem(sys.modules, "httpx", httpx_stub)
    assert server.cmd_status(args) == 0
    assert server.cmd_sessions(args) == 0

    monkeypatch.setitem(sys.modules, "httpx", httpx_stub)
    assert server.cmd_config(args) == 0
    assert "gateway" in capsys.readouterr().out


def test_main_help(monkeypatch, capsys):
    monkeypatch.setattr(server, "build_parser", lambda: type("P", (), {"parse_args": lambda _self: type("A", (), {"command": None})(), "print_help": lambda _self: print("help")})())
    assert server.main() == 2
    assert "help" in capsys.readouterr().out

def test_is_authorized_token_variants():
    cfg = server.load_config(None)
    cfg["gateway"]["auth"]["mode"] = "token"
    cfg["gateway"]["auth"]["token"]["value"] = "tok"
    ws = type("WS", (), {"headers": {"authorization": "Bearer tok"}, "query_params": {}, "client": None})()
    assert server.is_authorized(ws, cfg)[0] is True
    ws = type("WS", (), {"headers": {"authorization": ""}, "query_params": {}, "client": None})()
    assert server.is_authorized(ws, cfg)[0] is False


def test_is_http_authorized_password_variants():
    cfg = server.load_config(None)
    cfg["gateway"]["auth"]["mode"] = "password"
    cfg["gateway"]["auth"]["password"]["value"] = "pw"
    header = "Basic " + base64.b64encode(b"user:pw").decode("utf-8")
    req = type("Req", (), {"headers": {"authorization": header}, "query_params": {}})()
    assert server.is_http_authorized(req, cfg)[0] is True
    req = type("Req", (), {"headers": {"authorization": "Basic bad"}, "query_params": {}})()
    assert server.is_http_authorized(req, cfg)[0] is False
