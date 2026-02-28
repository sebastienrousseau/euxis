import asyncio
import json
import pytest
from gateway import server

class _FakeWebSocket:
    def __init__(self, headers=None, query_params=None):
        self.headers = headers or {}
        self.query_params = query_params or {}
        self.accepted = False
        self.closed = False
        self.close_code = None
        self.messages = []

    async def accept(self):
        self.accepted = True

    async def send_text(self, text):
        self.messages.append(text)

    async def close(self, code=1000):
        self.closed = True
        self.close_code = code


def _mcp_endpoint(app):
    for route in app.routes:
        if getattr(route, "path", None) == "/mcp":
            return route.endpoint
    raise AssertionError("MCP websocket endpoint not found")


def test_mcp_endpoint_disabled():
    cfg = server.load_config(None)
    cfg["gateway"]["mcp_enabled"] = False
    app = server.build_app(cfg)
    endpoint = _mcp_endpoint(app)
    ws = _FakeWebSocket()

    asyncio.run(endpoint(ws))

    assert ws.accepted is True
    assert ws.closed is True
    assert ws.close_code == 4404
    assert ws.messages
    payload = json.loads(ws.messages[0])
    assert payload["error"] == "mcp_disabled"

def test_mcp_endpoint_unauthorized():
    cfg = server.load_config(None)
    cfg["gateway"]["mcp_enabled"] = True
    cfg["gateway"]["auth"]["mode"] = "token"
    cfg["gateway"]["auth"]["token"]["value"] = "secret"
    app = server.build_app(cfg)
    endpoint = _mcp_endpoint(app)
    ws = _FakeWebSocket(headers={}, query_params={})

    asyncio.run(endpoint(ws))

    assert ws.accepted is True
    assert ws.closed is True
    assert ws.close_code == 4401
    assert ws.messages
    payload = json.loads(ws.messages[0])
    assert payload["error"]["message"] == "invalid_token"

@pytest.mark.asyncio
async def test_handle_frame_node_register():
    ws = AsyncMock()
    ws.send_text = AsyncMock()
    cfg = server.load_config(None)
    seq = {"value": 0}
    
    # test node.register
    frame = json.dumps({
        "type": "request",
        "id": "1",
        "method": "node.register",
        "params": {"hostname": "test", "platform": "linux", "capabilities": []}
    })
    
    await server.handle_frame(ws, frame, seq, cfg, "conn_1")
    assert "node_conn_1" in server.STATE.device_nodes
    
    # test node.* routing when nodes exist
    frame2 = json.dumps({
        "type": "request",
        "id": "2",
        "method": "node.shell",
        "params": {"command": "ls"}
    })
    await server.handle_frame(ws, frame2, seq, cfg, "conn_2")
    assert ws.send_text.called

@pytest.mark.asyncio
async def test_handle_frame_node_unavailable():
    ws = AsyncMock()
    ws.send_text = AsyncMock()
    cfg = server.load_config(None)
    seq = {"value": 0}
    server.STATE.device_nodes.clear()
    
    frame = json.dumps({
        "type": "request",
        "id": "1",
        "method": "node.shell",
        "params": {"command": "ls"}
    })
    
    await server.handle_frame(ws, frame, seq, cfg, "conn_1")
    call_args = ws.send_text.call_args[0][0]
    assert "NODE_UNAVAILABLE" in call_args

@pytest.mark.asyncio
async def test_handle_frame_event_broadcast():
    ws_node = AsyncMock()
    ws_client = AsyncMock()
    ws_client.send_text = AsyncMock()
    cfg = server.load_config(None)
    seq = {"value": 0}
    
    server.STATE.connections = {
        "conn_node": ws_node,
        "conn_client": ws_client
    }
    
    frame = json.dumps({
        "type": "event",
        "event": "node.browser.stream_frame",
        "data": {"screenshot": "base64..."}
    })
    
    await server.handle_frame(ws_node, frame, seq, cfg, "conn_node")
    assert ws_client.send_text.called

class AsyncMock:
    def __init__(self, *args, **kwargs):
        self.call_args = None
        self.called = False
    
    async def __call__(self, *args, **kwargs):
        self.called = True
        self.call_args = (args, kwargs)
        return None
