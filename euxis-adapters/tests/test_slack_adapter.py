# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import json
from urllib import error as urlerror

from adapters.slack_adapter import SlackAdapter, SlackAdapterConfig


def test_connect_non_socket_noop() -> None:
    adapter = SlackAdapter(SlackAdapterConfig(token="t", app_token="a", mode="webhook"))
    adapter.connect()
    assert adapter._socket_client is None


def test_connect_missing_tokens(monkeypatch) -> None:
    warnings = []
    monkeypatch.setattr("adapters.slack_adapter.LOGGER.warning", lambda msg, *_args: warnings.append(msg))
    adapter = SlackAdapter(SlackAdapterConfig(mode="socket"))
    adapter.connect()
    assert warnings


def test_connect_import_failure(monkeypatch) -> None:
    real_import = __import__

    def fake_import(name, *args, **kwargs):
        if name.startswith("slack_sdk"):
            raise ImportError("missing")
        return real_import(name, *args, **kwargs)

    monkeypatch.setattr("builtins.__import__", fake_import)
    adapter = SlackAdapter(SlackAdapterConfig(token="t", app_token="a", mode="socket"))
    adapter.connect()
    assert adapter._socket_client is None


def test_connect_success_listener_and_ack(monkeypatch) -> None:
    class FakeResponse:
        def __init__(self, envelope_id):
            self.envelope_id = envelope_id

    class FakeClient:
        def __init__(self, app_token=None, web_client=None):
            self.app_token = app_token
            self.web_client = web_client
            self.socket_mode_request_listeners = []
            self.connected = False
            self.acked = []

        def connect(self):
            self.connected = True

        def send_socket_mode_response(self, resp):
            self.acked.append(resp.envelope_id)

    class FakeWebClient:
        def __init__(self, token=None):
            self.token = token

    monkeypatch.setitem(__import__("sys").modules, "slack_sdk.socket_mode", type("m", (), {"SocketModeClient": FakeClient}))
    monkeypatch.setitem(
        __import__("sys").modules,
        "slack_sdk.socket_mode.response",
        type("m", (), {"SocketModeResponse": FakeResponse}),
    )
    monkeypatch.setitem(__import__("sys").modules, "slack_sdk.web", type("m", (), {"WebClient": FakeWebClient}))

    seen = []
    adapter = SlackAdapter(SlackAdapterConfig(token="t", app_token="a", mode="socket"), on_message=lambda *a: seen.append(a))
    monkeypatch.setattr("adapters.slack_adapter.persist_message", lambda *_args, **_kwargs: None)
    monkeypatch.setattr("adapters.slack_adapter.timestamp", lambda: "ts")
    adapter.connect()
    assert adapter._socket_client is not None
    # non-events payload should be ignored
    req_non_events = type("Req", (), {"type": "hello", "payload": {}, "envelope_id": "skip"})
    adapter._socket_client.socket_mode_request_listeners[0](req_non_events)
    req = type("Req", (), {"type": "events_api", "payload": {"event": {"channel": "C1", "text": "hi"}}, "envelope_id": "e1"})
    adapter._socket_client.socket_mode_request_listeners[0](req)
    assert seen
    assert adapter._socket_client.acked == ["e1"]


def test_receive_filters_and_persists(monkeypatch) -> None:
    persisted = []
    calls = []
    monkeypatch.setattr("adapters.slack_adapter.persist_message", lambda sid, entry: persisted.append((sid, entry)))
    monkeypatch.setattr("adapters.slack_adapter.timestamp", lambda: "fixed-ts")

    adapter = SlackAdapter(SlackAdapterConfig(token="t"), on_message=lambda *a: calls.append(a))
    adapter.receive("not-a-dict")
    assert persisted == []
    adapter.receive({"event": {"subtype": "bot_message"}})
    assert persisted == []

    adapter.receive({"event": {"channel": "C1", "thread_ts": "th1", "text": "hello", "user": "U1", "channel_type": "channel"}})
    assert persisted[0][0] == "slack_C1:th1"
    assert persisted[0][1]["meta"]["scope"] == "group"
    assert calls and calls[0][0] == "slack_C1:th1"


def test_send_and_disconnect_paths(monkeypatch) -> None:
    warnings = []
    monkeypatch.setattr("adapters.slack_adapter.LOGGER.warning", lambda msg, *_args: warnings.append(str(msg)))
    adapter = SlackAdapter(SlackAdapterConfig(token=""))
    adapter.send("msg", "slack_C1")
    assert warnings

    adapter = SlackAdapter(SlackAdapterConfig(token="tok"))
    adapter.send("msg", "")
    assert len(warnings) >= 2

    # success but API returns ok=false
    class Resp:
        def __init__(self, payload):
            self._payload = payload

        def __enter__(self):
            return self

        def __exit__(self, *_args):
            return False

        def read(self):
            return self._payload

    monkeypatch.setattr(
        "adapters.slack_adapter.urlrequest.urlopen",
        lambda _req, timeout=10: Resp(json.dumps({"ok": False}).encode("utf-8")),
    )
    adapter.send("msg", "slack_C1:thread1")

    monkeypatch.setattr(
        "adapters.slack_adapter.urlrequest.urlopen",
        lambda _req, timeout=10: (_ for _ in ()).throw(urlerror.URLError("bad")),
    )
    adapter.send("msg", "slack_C1")
    adapter.ack("m1")

    class SocketClient:
        def disconnect(self):
            raise RuntimeError("fail")

    adapter._socket_client = SocketClient()
    adapter.disconnect()
    adapter._socket_client = None
    adapter.disconnect()
    assert warnings


def test_resolve_session_variants() -> None:
    adapter = SlackAdapter(SlackAdapterConfig(token="t"))
    adapter.session_meta["s1"] = {"channel": "C1", "thread_ts": "T1"}
    assert adapter._resolve_session("s1") == ("C1", "T1")
    assert adapter._resolve_session("slack:C2:T2") == ("C2", "T2")
    assert adapter._resolve_session("slack:C3") == ("C3", None)
    assert adapter._resolve_session("slack_C4:T4") == ("C4", "T4")
    assert adapter._resolve_session("C5") == ("C5", None)
