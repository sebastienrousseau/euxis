import asyncio
import base64
import json
from types import SimpleNamespace

import pytest
from starlette.datastructures import QueryParams

from gateway import server


@pytest.fixture(autouse=True)
def reset_state():
    server.STATE = server.GatewayState()
    server._rate_limit_state.clear()
    server._auth_failure_state.clear()
    yield


def test_is_authorized_token_missing_and_invalid(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = ""
    ws = SimpleNamespace(headers={}, query_params={}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is False
    assert reason == "auth_not_configured"

    config["gateway"]["auth"]["token"]["value"] = "tok"
    ws = SimpleNamespace(headers={"authorization": "Bearer nope"}, query_params={}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is False
    assert reason == "invalid_token"

    monkeypatch.setattr(server, "audit_log", lambda *_args, **_kwargs: None)
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    ws = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is True
    assert reason is None


def test_is_authorized_password_invalid():
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "password"
    config["gateway"]["auth"]["password"]["value"] = "pw"

    ws = SimpleNamespace(headers={"authorization": "Bearer tok"}, query_params={}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is False
    assert reason == "missing_basic"

    ws = SimpleNamespace(headers={"authorization": "Basic not-base64"}, query_params={}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is False
    assert reason == "invalid_basic"

    creds = base64.b64encode(b"userpw").decode("utf-8")
    ws = SimpleNamespace(headers={"authorization": f"Basic {creds}"}, query_params={}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is False
    assert reason == "invalid_basic"


def test_is_http_authorized_invalid():
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = ""
    req = SimpleNamespace(headers={}, query_params={}, client=None)
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is False
    assert reason == "auth_not_configured"

    config["gateway"]["auth"]["token"]["value"] = "tok"
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is False
    assert reason == "invalid_token"

    config["gateway"]["auth"]["mode"] = "password"
    config["gateway"]["auth"]["password"]["value"] = "pw"
    req = SimpleNamespace(headers={"authorization": "Bearer tok"}, query_params={}, client=None)
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is False
    assert reason == "missing_basic"

    req = SimpleNamespace(headers={"authorization": "Basic not-base64"}, query_params={}, client=None)
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is False
    assert reason == "invalid_basic"

    creds = base64.b64encode(b"userpw").decode("utf-8")
    req = SimpleNamespace(headers={"authorization": f"Basic {creds}"}, query_params={}, client=None)
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is False
    assert reason == "invalid_basic"


def test_is_authorized_query_and_password_missing(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    monkeypatch.setattr(server, "audit_log", lambda *_args, **_kwargs: None)
    ws = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is True
    assert reason is None

    config["gateway"]["auth"]["mode"] = "password"
    config["gateway"]["auth"]["password"]["value"] = ""
    ws = SimpleNamespace(headers={}, query_params={}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is False
    assert reason == "auth_not_configured"

    config["gateway"]["auth"]["mode"] = "weird"
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is False
    assert reason == "auth_not_configured"


def test_is_http_authorized_query_and_password_missing(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    monkeypatch.setattr(server, "audit_log", lambda *_args, **_kwargs: None)
    req = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is True
    assert reason is None

    config["gateway"]["auth"]["mode"] = "password"
    config["gateway"]["auth"]["password"]["value"] = ""
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is False
    assert reason == "auth_not_configured"

    config["gateway"]["auth"]["mode"] = "weird"
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is False
    assert reason == "auth_not_configured"


def test_query_auth_audit(monkeypatch, capsys):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    ws = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    allowed, _ = server.is_authorized(ws, config)
    assert allowed is True
    assert events
    assert "SECURITY WARNING" in capsys.readouterr().err


def test_query_auth_http_audit(monkeypatch, capsys):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    req = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    allowed, _ = server.is_http_authorized(req, config)
    assert allowed is True
    assert events
    assert "SECURITY WARNING" in capsys.readouterr().err


def test_query_auth_is_authorized_direct(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))

    class DummyWS:
        headers = {}
        query_params = {"token": "tok"}

    allowed, _ = server.is_authorized(DummyWS(), config)
    assert allowed is True
    assert events


def test_query_auth_is_http_authorized_direct(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    req = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    allowed, _ = server.is_http_authorized(req, config)
    assert allowed is True
    assert events


def test_query_auth_is_authorized_get(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))

    class QueryParams:
        def get(self, key, default=None):
            return "tok" if key == "token" else default

    class DummyWS:
        headers = {}
        query_params = QueryParams()

    allowed, _ = server.is_authorized(DummyWS(), config)
    assert allowed is True
    assert events


def test_query_auth_is_http_authorized_get(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))

    class QueryParams:
        def get(self, key, default=None):
            return "tok" if key == "token" else default

    req = SimpleNamespace(headers={}, query_params=QueryParams(), client=None)
    allowed, _ = server.is_http_authorized(req, config)
    assert allowed is True
    assert events


def test_query_auth_is_authorized_match(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))

    class DummyWS:
        headers = {}

        class Query:
            def get(self, key, default=None):
                return "tok" if key == "token" else default

        query_params = Query()

    allowed, _ = server.is_authorized(DummyWS(), config)
    assert allowed is True
    assert events


def test_query_auth_is_http_authorized_match(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))

    class Query:
        def get(self, key, default=None):
            return "tok" if key == "token" else default

    req = SimpleNamespace(headers={}, query_params=Query(), client=None)
    allowed, _ = server.is_http_authorized(req, config)
    assert allowed is True
    assert events


def test_is_authorized_query_param_mismatch(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    monkeypatch.setattr(server, "audit_log", lambda *_args, **_kwargs: None)
    ws = SimpleNamespace(headers={}, query_params={"token": "nope"}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is False
    assert reason == "invalid_token"


def test_is_http_authorized_query_param_mismatch(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    monkeypatch.setattr(server, "audit_log", lambda *_args, **_kwargs: None)
    req = SimpleNamespace(headers={}, query_params={"token": "nope"}, client=None)
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is False
    assert reason == "invalid_token"


def test_is_authorized_query_branch(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    ws = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    allowed, reason = server.is_authorized(ws, config)
    assert allowed is True
    assert reason is None
    assert events


def test_is_http_authorized_query_branch(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    req = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    allowed, reason = server.is_http_authorized(req, config)
    assert allowed is True
    assert reason is None
    assert events


def test_is_authorized_query_branch_raises(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True

    def boom(_payload):
        raise RuntimeError("hit")

    monkeypatch.setattr(server, "audit_log", boom)
    ws = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    with pytest.raises(RuntimeError):
        server.is_authorized(ws, config)


def test_is_http_authorized_query_branch_raises(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True

    def boom(_payload):
        raise RuntimeError("hit")

    monkeypatch.setattr(server, "audit_log", boom)
    req = SimpleNamespace(headers={}, query_params={"token": "tok"}, client=None)
    with pytest.raises(RuntimeError):
        server.is_http_authorized(req, config)


def test_is_authorized_queryparams_starlette(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    ws = SimpleNamespace(headers={}, query_params=QueryParams("token=tok"), client=None)
    allowed, _ = server.is_authorized(ws, config)
    assert allowed is True
    assert events


def test_is_http_authorized_queryparams_starlette(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "tok"
    config["gateway"]["auth"]["token"]["allow_query_param"] = True
    events = []
    monkeypatch.setattr(server, "audit_log", lambda payload: events.append(payload))
    req = SimpleNamespace(headers={}, query_params=QueryParams("token=tok"), client=None)
    allowed, _ = server.is_http_authorized(req, config)
    assert allowed is True
    assert events




def test_verify_slack_signature_stale(monkeypatch):
    now = int(server.time.time())
    old = str(now - 1000)
    request = SimpleNamespace(headers={"X-Slack-Request-Timestamp": old, "X-Slack-Signature": "sig"})
    assert server.verify_slack_signature(request, b"body", "secret") is False


def test_check_rate_limit_allows():
    client = "10.0.0.1"
    assert server._check_rate_limit(client) is True
    assert server._rate_limit_state[client]


def test_run_voice_command_guardrails(monkeypatch):
    server.STATE.config = {"gateway": {"voice": {"command_allowlist": []}}}
    assert asyncio.run(server.run_voice_command("", {"text": "hi"})) == ""
    assert asyncio.run(server.run_voice_command("echo {text}", {"text": "hi"})) == ""

    server.STATE.config = {"gateway": {"voice": {"command_allowlist": ["allowed"]}}}
    assert asyncio.run(server.run_voice_command("echo {text}", {"text": "hi"})) == ""
    assert asyncio.run(server.run_voice_command("allowed {missing}", {"text": "hi"})) == ""

    server.STATE.config = {"gateway": {"voice": {"command_allowlist": [""]}}}
    assert asyncio.run(server.run_voice_command("{text}", {"text": ""})) == ""

    server.STATE.config = {"gateway": {"voice": {"command_allowlist": ["echo"]}}}
    assert asyncio.run(server.run_voice_command("echo{suffix}", {"suffix": "x"})) == ""

    server.STATE.config = {"gateway": {"voice": {"command_allowlist": ["echo"]}}}
    assert asyncio.run(server.run_voice_command("echo '{text", {"text": "hi"})) == ""

    server.STATE.config = {"gateway": {"voice": {"command_allowlist": ["echo"]}}}
    monkeypatch.setattr(server.shlex, "split", lambda _value: [])
    assert asyncio.run(server.run_voice_command("echo {text}", {"text": "hi"})) == ""


def test_run_voice_command_nonzero_return(monkeypatch):
    server.STATE.config = {"gateway": {"voice": {"command_allowlist": ["echo"]}}}

    class DummyProc:
        returncode = 1

        async def communicate(self):
            return b"out", b""

    async def fake_exec(*_args, **_kwargs):
        return DummyProc()

    monkeypatch.setattr(asyncio, "create_subprocess_exec", fake_exec)
    assert asyncio.run(server.run_voice_command("echo {text}", {"text": "hi"})) == ""

    class DummyProc:
        returncode = 1

        async def communicate(self):
            return b"", b""

    async def fake_exec(*_args, **_kwargs):
        return DummyProc()

    server.STATE.config = {"gateway": {"voice": {"command_allowlist": ["echo"]}}}
    monkeypatch.setattr(asyncio, "create_subprocess_exec", fake_exec)
    assert asyncio.run(server.run_voice_command("echo {text}", {"text": "hi"})) == ""

    async def boom(*_args, **_kwargs):
        raise RuntimeError("fail")

    monkeypatch.setattr(asyncio, "create_subprocess_exec", boom)
    assert asyncio.run(server.run_voice_command("echo {text}", {"text": "hi"})) == ""


def test_deliver_to_channel_guardrails(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "slack"})
    server.STATE.adapters = {}
    asyncio.run(server.deliver_to_channel("sess", "hello"))

    class DummyAdapter:
        def send(self, *_args, **_kwargs):
            raise RuntimeError("boom")

    server.STATE.adapters = {"slack": DummyAdapter()}
    asyncio.run(server.deliver_to_channel("sess", "hello"))


def test_notify_webhooks_retry(monkeypatch):
    server.STATE.webhooks = [{"url": "http://example.com", "events": ["agent.final"]}]

    def boom(_req, timeout=None):
        raise server.urlerror.URLError("bad")

    async def fake_sleep(_):
        return None

    monkeypatch.setattr(server.urlrequest, "urlopen", boom)
    monkeypatch.setattr(server.asyncio, "sleep", fake_sleep)
    asyncio.run(server.notify_webhooks({"event": "agent", "data": {"status": "final"}}))


def test_notify_webhooks_skips_non_matching(monkeypatch):
    server.STATE.webhooks = [{"url": "http://example.com", "events": ["agent.error"]}]
    asyncio.run(server.notify_webhooks({"event": "agent", "data": {"status": "final"}}))


def test_post_json_success(monkeypatch):
    class DummyResp:
        def __enter__(self):
            return self

        def __exit__(self, *_args):
            return False

        def read(self):
            return b""

    def fake_urlopen(_req, timeout=None):
        return DummyResp()

    monkeypatch.setattr(server.urlrequest, "urlopen", fake_urlopen)
    asyncio.run(server.post_json("http://example.com", {"ok": True}))


def test_process_inbound_policy_and_exec(monkeypatch):
    cfg = server.load_config(None)
    cfg["gateway"]["policy"]["mention_required"] = True
    cfg["gateway"]["exec"]["ask"] = "always"
    cfg["gateway"]["exec"]["ask_fallback"] = "deny"

    monkeypatch.setattr(server, "persist_message", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_session_meta", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "audit_log", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_approval", lambda *_args, **_kwargs: None)

    meta = {"channel_id": "channel", "scope": "channel", "sender": "bob", "mentions_bot": False}
    asyncio.run(server.process_inbound("sess", "hi", meta, cfg))

    meta = {"channel_id": "channel", "scope": "channel", "sender": "bob", "mentions_bot": True, "agent": "a"}
    asyncio.run(server.process_inbound("sess", "hi", meta, cfg))
    assert server.STATE.pending_approvals


def test_process_inbound_with_connection(monkeypatch):
    cfg = server.load_config(None)
    cfg["gateway"]["policy"]["mention_required"] = False
    cfg["gateway"]["exec"]["ask"] = "off"
    cfg["gateway"]["exec"]["ask_fallback"] = "full"

    monkeypatch.setattr(server, "persist_message", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_session_meta", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "load_session_meta", lambda *_args, **_kwargs: {})

    class DummyWS:
        async def send_text(self, _text):
            return None

    server.STATE.connections["conn"] = DummyWS()
    server.STATE.conn_seq["conn"] = 0

    async def fake_dispatch(*_args, **_kwargs):
        return None

    monkeypatch.setattr(server, "dispatch_with_lock", fake_dispatch)
    asyncio.run(server.process_inbound("sess", "hi", {"agent": "a"}, cfg))
    assert server.STATE.running


def test_is_exec_allowed_elevated_modes():
    cfg = server.load_config(None)
    cfg["gateway"]["exec"]["policy"] = "allowlist"
    cfg["gateway"]["exec"]["allowlist"] = []
    cfg["gateway"]["exec"]["elevated"] = "full"
    assert server.is_exec_allowed({"elevated": "full"}, cfg) is True
    cfg["gateway"]["exec"]["elevated"] = "off"
    assert server.is_exec_allowed({"elevated": "full"}, cfg) is False
    cfg["gateway"]["exec"]["ask"] = "always"
    cfg["gateway"]["exec"]["ask_fallback"] = "deny"
    assert server.is_exec_allowed({"agent": "a"}, cfg) is False
    cfg["gateway"]["exec"]["ask"] = "on-miss"
    assert server.is_exec_allowed({"agent": "a"}, cfg) is False
    cfg["gateway"]["exec"]["ask_fallback"] = "full"
    assert server.is_exec_allowed({"agent": "a"}, cfg) is False


def test_cron_loop_runs_once(monkeypatch):
    cfg = server.load_config(None)
    server.STATE.cron_jobs = [
        {"job_id": "bad", "every_seconds": 0, "session_id": "sess", "content": "hi"},
        {"job_id": "skip", "every_seconds": 60, "last_run": server.time.time(), "session_id": "sess", "content": "hi"},
        {"job_id": "missing", "every_seconds": 1, "session_id": "", "content": "hi", "meta": {"m": 1}},
        {"job_id": "run", "every_seconds": 1, "session_id": "sess", "content": "hi", "meta": {"m": 1}},
    ]
    calls = {"persist": 0, "cleanup": 0, "process": 0}

    monkeypatch.setattr(server, "persist_cron_jobs", lambda *_args, **_kwargs: calls.__setitem__("persist", calls["persist"] + 1))
    monkeypatch.setattr(server, "cleanup_voice", lambda *_args, **_kwargs: calls.__setitem__("cleanup", calls["cleanup"] + 1))

    async def fake_process(*_args, **_kwargs):
        calls["process"] += 1

    monkeypatch.setattr(server, "process_inbound", fake_process)

    times = [4000, 4000]

    def fake_time():
        return times.pop(0) if times else 4000

    monkeypatch.setattr(server.time, "time", fake_time)

    async def fake_sleep(_):
        raise asyncio.CancelledError

    monkeypatch.setattr(asyncio, "sleep", fake_sleep)
    asyncio.run(server.cron_loop(cfg))
    assert calls["persist"] >= 1
    assert calls["cleanup"] == 1
    assert calls["process"] == 1


def test_cron_loop_no_cleanup(monkeypatch):
    cfg = server.load_config(None)
    server.STATE.cron_jobs = []
    calls = {"cleanup": 0}

    monkeypatch.setattr(server, "cleanup_voice", lambda *_args, **_kwargs: calls.__setitem__("cleanup", calls["cleanup"] + 1))

    times = [10, 11]

    def fake_time():
        return times.pop(0) if times else 11

    monkeypatch.setattr(server.time, "time", fake_time)

    async def fake_sleep(_):
        raise asyncio.CancelledError

    monkeypatch.setattr(asyncio, "sleep", fake_sleep)
    asyncio.run(server.cron_loop(cfg))
    assert calls["cleanup"] == 0


def test_send_ticks_once(monkeypatch):
    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    calls = {"sleep": 0}

    async def fake_sleep(_):
        calls["sleep"] += 1
        if calls["sleep"] > 1:
            raise asyncio.CancelledError

    monkeypatch.setattr(asyncio, "sleep", fake_sleep)
    asyncio.run(server.send_ticks(DummyWS(), {"value": 0}))
    assert sent and sent[0]["event"] == "tick"


def test_process_inbound_no_connections(monkeypatch):
    cfg = server.load_config(None)
    cfg["gateway"]["policy"]["mention_required"] = False
    cfg["gateway"]["exec"]["ask"] = "off"
    cfg["gateway"]["exec"]["ask_fallback"] = "full"
    monkeypatch.setattr(server, "persist_message", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "persist_session_meta", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "load_session_meta", lambda *_args, **_kwargs: {})
    asyncio.run(server.process_inbound("sess", "hi", {"agent": "a"}, cfg))
    assert server.STATE.running == {}


def test_is_exec_allowed_approved():
    cfg = server.load_config(None)
    cfg["gateway"]["exec"]["policy"] = "allowlist"
    cfg["gateway"]["exec"]["allowlist"] = []
    cfg["gateway"]["exec"]["ask"] = "always"
    cfg["gateway"]["exec"]["ask_fallback"] = "deny"
    assert server.is_exec_allowed({"approved": True}, cfg) is True


def test_is_exec_allowed_elevated_ask_mode():
    cfg = server.load_config(None)
    cfg["gateway"]["exec"]["policy"] = "allowlist"
    cfg["gateway"]["exec"]["allowlist"] = []
    cfg["gateway"]["exec"]["elevated"] = "ask"
    cfg["gateway"]["exec"]["ask"] = "off"
    cfg["gateway"]["exec"]["ask_fallback"] = "full"
    assert server.is_exec_allowed({"elevated": "full", "agent": "x"}, cfg) is True


def test_send_agent_event_triggers_hooks(monkeypatch):
    sent = []
    called = {"hooks": 0, "deliver": 0, "tts": 0}

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    monkeypatch.setattr(server, "persist_run_event", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "notify_webhooks", lambda *_args, **_kwargs: asyncio.sleep(0))
    monkeypatch.setattr(server, "deliver_to_channel", lambda *_args, **_kwargs: asyncio.sleep(0))
    monkeypatch.setattr(server, "push_voice_tts", lambda *_args, **_kwargs: asyncio.sleep(0))

    asyncio.run(server.send_agent_event(DummyWS(), {"value": 0}, "run", "sess", "agent", "final", "done"))
    assert sent[0]["event"] == "agent"


def test_cmd_status_and_sessions_failure(monkeypatch, capsys):
    args = type("Args", (), {"bind": "127.0.0.1", "port": 10000, "config": None, "auth_mode": None})()
    monkeypatch.setattr(server, "resolve_config", lambda _args: {"gateway": {"bind": "127.0.0.1", "port": 1, "health_path": "/health", "channels": {}}})

    class Boom:
        def get(self, *_args, **_kwargs):
            raise RuntimeError("down")

    monkeypatch.setitem(server.sys.modules, "httpx", Boom())
    assert server.cmd_status(args) == 1
    assert server.cmd_sessions(args) == 1
    out = capsys.readouterr().out
    assert "Gateway not responding" in out


def test_cmd_status_non_200(monkeypatch, capsys):
    args = type("Args", (), {"bind": "127.0.0.1", "port": 10000, "config": None, "auth_mode": None})()
    monkeypatch.setattr(server, "resolve_config", lambda _args: {"gateway": {"bind": "127.0.0.1", "port": 1, "health_path": "/health", "channels": {}}})

    class DummyResp:
        status_code = 500
        text = "bad"

    httpx_stub = type("httpx", (), {"get": lambda *_args, **_kwargs: DummyResp()})
    monkeypatch.setitem(server.sys.modules, "httpx", httpx_stub)
    assert server.cmd_status(args) == 1
    assert server.cmd_sessions(args) == 1
    assert "bad" in capsys.readouterr().out


def test_main_default_help(monkeypatch, capsys):
    monkeypatch.setattr(server.sys, "argv", ["server.py"])
    result = server.main()
    assert result == 2
    assert "usage" in capsys.readouterr().out.lower()


def test_main_command_dispatch(monkeypatch):
    monkeypatch.setattr(server, "cmd_run", lambda _args: 1)
    monkeypatch.setattr(server, "cmd_status", lambda _args: 2)
    monkeypatch.setattr(server, "cmd_config", lambda _args: 3)
    monkeypatch.setattr(server, "cmd_sessions", lambda _args: 4)

    monkeypatch.setattr(server.sys, "argv", ["server.py", "run"])
    assert server.main() == 1
    monkeypatch.setattr(server.sys, "argv", ["server.py", "status"])
    assert server.main() == 2
    monkeypatch.setattr(server.sys, "argv", ["server.py", "config"])
    assert server.main() == 3
    monkeypatch.setattr(server.sys, "argv", ["server.py", "sessions"])
    assert server.main() == 4


def test_push_voice_tts_non_voice(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "webchat"})
    asyncio.run(server.push_voice_tts("sess", "hello"))


def test_push_voice_tts_voice_session(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {"mode": "command", "command": "echo {text}"}, "command_allowlist": ["echo"]}}}

    async def fake_run(_cmd, _vars):
        return "/tmp/audio.wav"

    monkeypatch.setattr(server, "run_voice_command", fake_run)

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.push_voice_tts("sess", "hello"))
    assert sent and sent[0]["audio_path"] == "/tmp/audio.wav"


def test_push_voice_tts_audio_path(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {"mode": "command", "command": "echo {text}"}, "command_allowlist": ["echo"]}}}

    async def fake_run(_cmd, _vars):
        return "/tmp/audio2.wav"

    monkeypatch.setattr(server, "run_voice_command", fake_run)

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.push_voice_tts("sess", "hello"))
    assert sent and sent[0]["audio_path"] == "/tmp/audio2.wav"


def test_push_voice_tts_audio_path_direct(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {"mode": "command", "command": "echo {text}"}, "command_allowlist": ["echo"]}}}

    async def fake_run(_cmd, _vars):
        return "/tmp/audio3.wav"

    monkeypatch.setattr(server, "run_voice_command", fake_run)

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.push_voice_tts("sess", "hello"))
    assert sent and sent[0]["audio_path"] == "/tmp/audio3.wav"


def test_push_voice_tts_audio_path_branch(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {"mode": "command", "command": "echo {text}"}, "command_allowlist": ["echo"]}}}

    async def fake_run(_cmd, _vars):
        return "/tmp/audio4.wav"

    monkeypatch.setattr(server, "run_voice_command", fake_run)

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.push_voice_tts("sess", "hello"))
    assert sent and sent[0]["audio_path"] == "/tmp/audio4.wav"


def test_push_voice_tts_audio_path_match(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {"mode": "command", "command": "echo {text}"}, "command_allowlist": ["echo"]}}}

    async def fake_run(_cmd, _vars):
        return "audio.wav"

    monkeypatch.setattr(server, "run_voice_command", fake_run)

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.push_voice_tts("sess", "hello"))
    assert sent and sent[0]["audio_path"] == "audio.wav"


def test_push_voice_tts_audio_path_via_send_agent_event(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {"mode": "command", "command": "echo {text}"}, "command_allowlist": ["echo"]}}}

    async def fake_run(_cmd, _vars):
        return "audio-final.wav"

    monkeypatch.setattr(server, "run_voice_command", fake_run)
    monkeypatch.setattr(server, "persist_run_event", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(server, "notify_webhooks", lambda *_args, **_kwargs: asyncio.sleep(0))
    monkeypatch.setattr(server, "deliver_to_channel", lambda *_args, **_kwargs: asyncio.sleep(0))

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.send_agent_event(DummyWS(), {"value": 0}, "run", "sess", "agent", "final", "done"))
    assert any("audio_path" in payload for payload in sent)


def test_push_voice_tts_audio_path_dumps(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {"mode": "command", "command": "echo {text}"}, "command_allowlist": ["echo"]}}}

    class DummyProc:
        returncode = 0

        async def communicate(self):
            return b"/tmp/audio-hit.wav", b""

    async def fake_exec(*_args, **_kwargs):
        return DummyProc()

    monkeypatch.setattr(asyncio, "create_subprocess_exec", fake_exec)

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.push_voice_tts("sess", "hello"))
    assert sent and sent[0]["audio_path"] == "/tmp/audio-hit.wav"


def test_push_voice_tts_audio_path_empty(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {"mode": "command", "command": "echo {text}"}, "command_allowlist": ["echo"]}}}

    async def fake_run(_cmd, _vars):
        return ""

    monkeypatch.setattr(server, "run_voice_command", fake_run)

    sent = []

    class DummyWS:
        async def send_text(self, text):
            sent.append(json.loads(text))

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.push_voice_tts("sess", "hello"))
    assert sent and "audio_path" not in sent[0]


def test_push_voice_tts_send_error(monkeypatch):
    monkeypatch.setattr(server, "load_session_meta", lambda _sid: {"channel_id": "voice"})
    server.STATE.config = {"gateway": {"voice": {"tts": {}}}}

    class DummyWS:
        async def send_text(self, _text):
            raise RuntimeError("boom")

    server.STATE.voice_connections["sess"] = [DummyWS()]
    asyncio.run(server.push_voice_tts("sess", "hello"))
