import asyncio
import json

import pytest

fastapi = pytest.importorskip("fastapi")
if not hasattr(fastapi, "__path__"):
    pytest.skip("real fastapi not available", allow_module_level=True)

from gateway import server


def _get_ws_endpoint(app, path):
    for route in app.routes:
        if getattr(route, "path", None) == path:
            return route.endpoint
    raise AssertionError("websocket endpoint not found")


class FakeWS:
    def __init__(self, headers=None, messages=None):
        self.headers = headers or {}
        self.query_params = {}
        self._messages = list(messages or [])
        self.accepted = False
        self.closed = False
        self.sent = []

    async def accept(self):
        self.accepted = True

    async def receive_text(self):
        if not self._messages:
            raise server.WebSocketDisconnect()
        return self._messages.pop(0)

    async def send_text(self, text):
        self.sent.append(json.loads(text))

    async def close(self, code=1000):
        self.closed = True
        self.close_code = code


def test_websocket_unauthorized():
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "token"
    config["gateway"]["auth"]["token"]["value"] = "secret"
    app = server.build_app(config)
    ws = FakeWS(headers={})
    endpoint = _get_ws_endpoint(app, "/")
    asyncio.run(endpoint(ws))
    assert ws.accepted is True
    assert ws.closed is True
    assert ws.sent[0]["error"]["code"] == "UNAUTHORIZED"


def test_websocket_authorized(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    app = server.build_app(config)
    ws = FakeWS(messages=["{}"])
    endpoint = _get_ws_endpoint(app, "/")

    handled = []

    async def fake_handle(_ws, raw, _seq, _cfg, _conn_id):
        handled.append(raw)

    monkeypatch.setattr(server, "handle_frame", fake_handle)
    asyncio.run(endpoint(ws))
    assert ws.accepted is True
    assert handled
