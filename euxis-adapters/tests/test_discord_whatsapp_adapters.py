# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import asyncio
import json
from urllib import error as urlerror

from adapters.discord_adapter import DiscordAdapter, DiscordAdapterConfig
from adapters.whatsapp_adapter import WhatsAppAdapter, WhatsAppAdapterConfig


def test_discord_connect_receive_send_disconnect(monkeypatch) -> None:
    infos = []
    warnings = []
    monkeypatch.setattr("adapters.discord_adapter.LOGGER.info", lambda msg, *_args: infos.append(str(msg)))
    monkeypatch.setattr("adapters.discord_adapter.LOGGER.warning", lambda msg, *_args: warnings.append(str(msg)))

    adapter = DiscordAdapter(DiscordAdapterConfig(enabled=False))
    adapter.connect()
    assert infos

    # import failure
    real_import = __import__

    def fake_import(name, *args, **kwargs):
        if name == "discord":
            raise ImportError("missing")
        return real_import(name, *args, **kwargs)

    monkeypatch.setattr("builtins.__import__", fake_import)
    adapter = DiscordAdapter(DiscordAdapterConfig(enabled=True, token="tok"))
    adapter.connect()
    assert warnings
    monkeypatch.setattr("builtins.__import__", real_import)

    # connect success path
    events = {}

    class FakeIntents:
        @staticmethod
        def default():
            return type("I", (), {"message_content": False})()

    class FakeClient:
        def __init__(self, intents=None):
            self.intents = intents
            self.user = object()

        def event(self, fn):
            events[fn.__name__] = fn
            return fn

        async def start(self, _token):
            return None

        def is_ready(self):
            return True

        def get_channel(self, _id):
            return None

        async def fetch_channel(self, _id):
            return None

        async def close(self):
            return None

    fake_discord = type("D", (), {"Intents": FakeIntents, "Client": FakeClient})

    class FakeLoop:
        def run_until_complete(self, coro):
            asyncio.run(coro)

    monkeypatch.setitem(__import__("sys").modules, "discord", fake_discord)
    monkeypatch.setattr("adapters.discord_adapter.asyncio.new_event_loop", lambda: FakeLoop())
    monkeypatch.setattr("adapters.discord_adapter.asyncio.set_event_loop", lambda _loop: None)
    class FakeThread:
        def __init__(self, target=None, daemon=True):
            self._target = target

        def start(self):
            if self._target:
                self._target()

    monkeypatch.setattr("adapters.discord_adapter.threading.Thread", FakeThread)
    adapter = DiscordAdapter(DiscordAdapterConfig(enabled=True, token="tok"))
    monkeypatch.setattr(adapter, "receive", lambda _msg: infos.append("received"))
    adapter.connect()
    assert adapter.client is not None
    # on_message branch: self-message ignored, other message dispatched
    msg = type("M", (), {"author": adapter.client.user})()
    asyncio.run(events["on_message"](msg))
    msg2 = type("M", (), {"author": object()})()
    asyncio.run(events["on_message"](msg2))
    assert "received" in infos
    asyncio.run(events["on_ready"]())

    # receive path
    persisted = []
    delivered = []
    monkeypatch.setattr("adapters.discord_adapter.persist_message", lambda sid, entry: persisted.append((sid, entry)))
    monkeypatch.setattr("adapters.discord_adapter.timestamp", lambda: "fixed-ts")
    adapter = DiscordAdapter(DiscordAdapterConfig(enabled=True, token="tok"), on_message=lambda *a: delivered.append(a))
    adapter.client = type("Client", (), {"user": object()})()
    message = type(
        "Msg",
        (),
        {
            "channel": type("Ch", (), {"id": 123, "type": "private"})(),
            "author": type("Au", (), {"id": 999})(),
            "content": "hello",
            "id": 7,
            "mentions": [],
        },
    )()
    adapter.receive(message)
    assert persisted and persisted[0][0] == "discord_123"
    assert delivered and delivered[0][0] == "discord_123"

    # send not ready
    adapter.client = None
    adapter.send("x", "discord_123")
    assert warnings

    class FakeChan:
        def __init__(self):
            self.sent = []

        async def send(self, m):
            self.sent.append(m)

    fake_chan = FakeChan()

    class FakeClient:
        def is_ready(self):
            return True

        def get_channel(self, _id):
            return None

        async def fetch_channel(self, _id):
            return fake_chan

    adapter.client = FakeClient()
    adapter.loop = object()
    monkeypatch.setattr("adapters.discord_adapter.asyncio.run_coroutine_threadsafe", lambda coro, loop: asyncio.run(coro))
    adapter.send("reply", "discord_123")
    assert fake_chan.sent == ["reply"]
    adapter.send("reply2", "unknown")
    adapter.ack("m1")

    class Closable:
        async def close(self):
            return None

    adapter.client = Closable()
    adapter.loop = object()
    adapter.disconnect()
    adapter.client = None
    adapter.loop = None
    adapter.disconnect()


def test_whatsapp_receive_send_and_disconnect(monkeypatch):
    infos = []
    warnings = []
    monkeypatch.setattr("adapters.whatsapp_adapter.LOGGER.info", lambda msg, *_args: infos.append(str(msg)))
    monkeypatch.setattr("adapters.whatsapp_adapter.LOGGER.warning", lambda msg, *_args: warnings.append(str(msg)))

    adapter = WhatsAppAdapter(WhatsAppAdapterConfig(enabled=False))
    adapter.connect()
    assert infos

    adapter = WhatsAppAdapter(WhatsAppAdapterConfig(enabled=True, token="tok"))
    adapter.connect()
    assert len(infos) >= 1

    persisted = []
    delivered = []
    monkeypatch.setattr("adapters.whatsapp_adapter.persist_message", lambda sid, entry: persisted.append((sid, entry)))
    monkeypatch.setattr("adapters.whatsapp_adapter.timestamp", lambda: "fixed-ts")
    adapter = WhatsAppAdapter(WhatsAppAdapterConfig(enabled=True, token="tok"), on_message=lambda *a: delivered.append(a))
    payload = {
        "entry": [
            {
                "changes": [
                    {"value": {"messages": [{"from": "555", "id": "m1", "text": {"body": "hello"}}]}},
                ]
            }
        ]
    }
    adapter.receive(payload)
    assert persisted and persisted[0][0] == "whatsapp_555"
    assert delivered and delivered[0][0] == "whatsapp_555"

    # invalid message ignored
    adapter._handle_single_message({"from": "", "text": {"body": ""}}, {})

    # missing config
    adapter2 = WhatsAppAdapter(WhatsAppAdapterConfig(token="", phone_number_id=""))
    adapter2.send("x", "whatsapp_555")
    assert warnings

    class Resp:
        def __init__(self, payload):
            self._payload = payload

        def __enter__(self):
            return self

        def __exit__(self, *_args):
            return False

        def read(self):
            return self._payload

    adapter3 = WhatsAppAdapter(WhatsAppAdapterConfig(token="tok", phone_number_id="pn"))
    monkeypatch.setattr(
        "adapters.whatsapp_adapter.urlrequest.urlopen",
        lambda _req, timeout=10: Resp(json.dumps({"error": {"message": "bad"}}).encode("utf-8")),
    )
    adapter3.send("msg", "whatsapp_777")

    monkeypatch.setattr(
        "adapters.whatsapp_adapter.urlrequest.urlopen",
        lambda _req, timeout=10: (_ for _ in ()).throw(urlerror.URLError("bad")),
    )
    adapter3.send("msg", "whatsapp_777")

    adapter3.send("msg", "bad")
    adapter3.ack("m1")
    adapter3.disconnect()
