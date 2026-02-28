import asyncio
import json
import pytest
from unittest.mock import AsyncMock, MagicMock
from fastapi import WebSocketDisconnect
from gateway.mcp import MCPHost

class DummyState:
    def __init__(self):
        self.started_at = 0
        self.sessions_active = 1

class DummyWS:
    def __init__(self, messages):
        self.messages = messages
        self.sent = []
        self.accept_called = False

    async def accept(self):
        self.accept_called = True

    async def receive_text(self):
        if not self.messages:
            raise WebSocketDisconnect()
        return self.messages.pop(0)

    async def send_text(self, text):
        self.sent.append(json.loads(text))

@pytest.mark.asyncio
async def test_mcp_host_initialization():
    state = DummyState()
    host = MCPHost(state, {})
    assert "get_metrics" in host.tools

@pytest.mark.asyncio
async def test_mcp_handle_connection():
    state = DummyState()
    host = MCPHost(state, {})
    
    messages = [
        json.dumps({"method": "not_initialize", "id": 1}),
        json.dumps({"method": "initialize", "id": 2, "params": {}}),
        json.dumps({"method": "notifications/initialized"}),
        json.dumps({"method": "tools/list", "id": 3}),
        json.dumps({"method": "tools/call", "id": 4, "params": {"name": "get_metrics"}}),
        json.dumps({"method": "tools/call", "id": 5, "params": {"name": "unknown_tool"}}),
        json.dumps({"method": "ping", "id": 6}),
        json.dumps({"method": "unknown_method", "id": 7}),
        "invalid_json"
    ]
    
    ws = DummyWS(messages)
    await host.handle_connection(ws)
    
    assert ws.accept_called
    assert ws.sent[0]["error"]["code"] == -32002  # Not initialized
    assert ws.sent[1]["result"]["serverInfo"]["name"] == "euxis-gateway"  # Initialize
    assert "tools" in ws.sent[2]["result"]  # tools/list
    assert "content" in ws.sent[3]["result"]  # get_metrics call
    assert ws.sent[4]["error"]["code"] == -32601  # Unknown tool
    assert ws.sent[5]["result"] == {}  # Ping
    assert ws.sent[6]["error"]["code"] == -32601  # Unknown method
    assert ws.sent[7]["error"]["code"] == -32700  # Parse error

@pytest.mark.asyncio
async def test_mcp_tool_execution_error():
    state = DummyState()
    host = MCPHost(state, {})
    
    async def bad_handler():
        raise Exception("Tool error")
        
    host.register_tool("bad_tool", "desc", {}, bad_handler)
    
    messages = [
        json.dumps({"method": "initialize", "id": 1}),
        json.dumps({"method": "tools/call", "id": 2, "params": {"name": "bad_tool"}})
    ]
    ws = DummyWS(messages)
    await host.handle_connection(ws)
    
    assert ws.sent[1]["error"]["code"] == -32603

@pytest.mark.asyncio
async def test_mcp_default_tools():
    state = DummyState()
    host = MCPHost(state, {})
    
    metrics = await host._tool_get_metrics("sess")
    assert "uptime_ms" in metrics
    
    sig = await host._tool_sign_payload("data", "key")
    assert "signature" in sig
