# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import json
from urllib import error as urlerror

from adapters.telegram_adapter import TelegramAdapter, TelegramAdapterConfig


def test_connect_modes_and_receive_send(monkeypatch) -> None:
    warnings = []
    monkeypatch.setattr("adapters.telegram_adapter.LOGGER.warning", lambda msg, *_args: warnings.append(str(msg)))

    # missing token
    adapter = TelegramAdapter(TelegramAdapterConfig(token=""))
    adapter.connect()
    assert warnings

    # webhook without URL
    adapter = TelegramAdapter(TelegramAdapterConfig(token="t", mode="webhook", webhook_url=""))
    adapter.connect()
    assert len(warnings) >= 2

    # webhook with URL
    calls = []
    adapter = TelegramAdapter(TelegramAdapterConfig(token="t", mode="webhook", webhook_url="https://x"))
    monkeypatch.setattr(adapter, "_api_call", lambda method, data: calls.append((method, data)) or {})
    adapter.connect()
    assert calls[0][0] == "setWebhook"

    # polling mode starts thread
    started = {"v": False}

    class FakeThread:
        def __init__(self, target=None, daemon=None):
            self._target = target
            self._alive = False

        def start(self):
            started["v"] = True
            self._alive = True

        def is_alive(self):
            return self._alive

        def join(self, timeout=None):
            self._alive = False

    monkeypatch.setattr("adapters.telegram_adapter.threading.Thread", FakeThread)
    adapter = TelegramAdapter(TelegramAdapterConfig(token="t", mode="poll"))
    adapter.connect()
    assert started["v"] is True
    adapter.connect()  # idempotent when alive

    persisted = []
    delivered = []
    monkeypatch.setattr("adapters.telegram_adapter.persist_message", lambda sid, entry: persisted.append((sid, entry)))
    monkeypatch.setattr("adapters.telegram_adapter.timestamp", lambda: "fixed-ts")
    adapter = TelegramAdapter(TelegramAdapterConfig(token="t"), on_message=lambda *a: delivered.append(a))
    adapter.receive("not-a-dict")
    adapter.receive({"message": {"chat": {"id": 123, "type": "group"}, "text": "hello", "message_id": 7, "from": {"id": 9}}})
    assert persisted[0][0] == "telegram_123"
    assert delivered

    adapter.receive({"edited_message": {"chat": {"id": 456, "type": "private"}, "caption": "cap"}})
    assert len(persisted) == 2
    adapter.receive({"message": {"chat": {}}})  # no chat id -> ignore

    send_calls = []
    monkeypatch.setattr(adapter, "_api_call", lambda method, data: send_calls.append((method, data)) or {})
    adapter.send("x", "telegram_123")
    assert send_calls and send_calls[0][0] == "sendMessage"
    adapter.send("x", "bad-id")
    adapter.ack("m1")

    adapter = TelegramAdapter(TelegramAdapterConfig(token=""))
    adapter.send("x", "telegram_1")
    assert warnings


def test_disconnect_poll_loop_and_helpers(monkeypatch) -> None:
    adapter = TelegramAdapter(TelegramAdapterConfig(token="t", mode="webhook"))
    calls = []
    monkeypatch.setattr(adapter, "_api_call", lambda method, data: calls.append((method, data)) or {})
    adapter.disconnect()
    assert calls and calls[0][0] == "deleteWebhook"

    class AliveThread:
        def __init__(self):
            self._alive = True

        def is_alive(self):
            return self._alive

        def join(self, timeout=None):
            self._alive = False

    adapter = TelegramAdapter(TelegramAdapterConfig(token="t", mode="poll"))
    adapter._poll_thread = AliveThread()
    adapter.disconnect()

    # _poll_loop single iteration with update processing
    adapter = TelegramAdapter(TelegramAdapterConfig(token="t", mode="poll", poll_interval=0.0))
    seen = []
    monkeypatch.setattr(adapter, "receive", lambda update: seen.append(update))
    monkeypatch.setattr(
        adapter,
        "_api_call",
        lambda method, payload: {"result": [{"update_id": 41, "message": {"chat": {"id": 1}, "text": "x"}}]},
    )
    monkeypatch.setattr("adapters.telegram_adapter.time.sleep", lambda _x: adapter._stop_event.set())
    adapter._poll_loop()
    assert adapter._offset == 42
    assert seen

    assert adapter._resolve_chat_id("telegram_99") == 99
    assert adapter._resolve_chat_id("telegram:88") == 88
    assert adapter._resolve_chat_id("bad") is None


def test_api_call_success_and_failures(monkeypatch):
    adapter = TelegramAdapter(TelegramAdapterConfig(token="t"))

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
        "adapters.telegram_adapter.urlrequest.urlopen",
        lambda _req, timeout=15: Resp(json.dumps({"ok": True, "result": []}).encode("utf-8")),
    )
    out = adapter._api_call("getMe", {})
    assert out["ok"] is True

    monkeypatch.setattr(
        "adapters.telegram_adapter.urlrequest.urlopen",
        lambda _req, timeout=15: (_ for _ in ()).throw(urlerror.URLError("bad")),
    )
    assert adapter._api_call("getMe", {}) == {}

    monkeypatch.setattr(
        "adapters.telegram_adapter.urlrequest.urlopen",
        lambda _req, timeout=15: Resp(b"{"),
    )
    assert adapter._api_call("getMe", {}) == {}
