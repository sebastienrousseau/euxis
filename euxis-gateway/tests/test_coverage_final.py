import asyncio
import json
import pytest
import os
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock
from gateway import server
from gateway.identity import AgentIdentity

@pytest.fixture(autouse=True)
def setup_state():
    server.STATE = server.GatewayState()
    yield

def test_is_exec_allowed_comprehensive(monkeypatch):
    cfg = {
        "gateway": {
            "exec": {
                "policy": "allowlist",
                "allowlist": [],
                "ask": "on-miss",
                "ask_fallback": "deny",
                "elevated": "ask"
            }
        }
    }
    
    def set_identity(trust):
        monkeypatch.setattr(server.STATE.identity_manager, "get_identity", 
                            lambda aid: AgentIdentity(aid, aid, trust_level=trust))

    # Trust levels
    set_identity("elevated")
    assert server.is_exec_allowed({"agent": "a"}, cfg) is True
    
    set_identity("restricted")
    assert server.is_exec_allowed({"agent": "a", "approved": False}, cfg) is False
    assert server.is_exec_allowed({"agent": "a", "approved": True}, cfg) is True
    
    set_identity("trusted")
    assert server.is_exec_allowed({"agent": "a"}, cfg) is True

    # Elevated modes (must be approved if restricted)
    set_identity("restricted")
    cfg["gateway"]["exec"]["elevated"] = "full"
    # Even if elevated=full, restricted but not approved returns False
    assert server.is_exec_allowed({"agent": "a", "elevated": "full", "approved": False}, cfg) is False
    assert server.is_exec_allowed({"agent": "a", "elevated": "full", "approved": True}, cfg) is True
    
    # Try with trusted identity and elevated modes
    set_identity("trusted")
    cfg["gateway"]["exec"]["elevated"] = "off"
    assert server.is_exec_allowed({"agent": "a", "elevated": "full"}, cfg) is False

def test_is_message_allowed_comprehensive():
    cfg = {
        "gateway": {
            "policy": {
                "mention_required": False,
                "allowlist": []
            }
        }
    }
    
    # DM
    assert server.is_message_allowed({"scope": "dm"}, cfg) is True
    
    # Allowlist
    cfg["gateway"]["policy"]["allowlist"] = ["user1"]
    assert server.is_message_allowed({"scope": "group", "sender": "user1"}, cfg) is True
    assert server.is_message_allowed({"scope": "group", "sender": "user2"}, cfg) is False
    
    # Mentions
    cfg["gateway"]["policy"]["allowlist"] = []
    cfg["gateway"]["policy"]["mention_required"] = True
    assert server.is_message_allowed({"scope": "group", "mentions_bot": True, "sender": "u1"}, cfg) is True
    assert server.is_message_allowed({"scope": "group", "mentions_bot": False, "sender": "u1"}, cfg) is False

@pytest.mark.asyncio
async def test_handle_frame_coverage_gap():
    ws = AsyncMock()
    ws.send_text = AsyncMock()
    cfg = server.load_config(None)
    
    # node.register success
    frame = json.dumps({
        "type": "request", "id": "r1", "method": "node.register",
        "params": {"hostname": "h", "platform": "p"}
    })
    await server.handle_frame(ws, frame, {"value": 0}, cfg, "c1")
    
    # event non-stream
    frame_evt = json.dumps({"type": "event", "event": "other"})
    await server.handle_frame(ws, frame_evt, {"value": 0}, cfg, "c1")

@pytest.mark.asyncio
async def test_dispatch_agent_wasm_mcp_disabled(monkeypatch):
    ws = AsyncMock()
    cfg = server.load_config(None)
    cfg["gateway"]["mcp_enabled"] = False
    meta = {"mode": "wasm", "plugin": "p.wasm", "agent": "a"}
    mock_proc = AsyncMock()
    mock_proc.wait = AsyncMock(return_value=0)
    mock_proc.returncode = 0
    mock_proc.communicate = AsyncMock(return_value=(b"out", b"err"))
    monkeypatch.setattr(asyncio, "create_subprocess_exec", AsyncMock(return_value=mock_proc))
    await server.dispatch_agent(ws, {"value": 0}, "r1", "s1", meta, "{}", cfg)

def test_build_app_webchat_mount(monkeypatch, tmp_path):
    webchat_dir = tmp_path / "webchat"
    webchat_dir.mkdir()
    
    class MockPath:
        def __init__(self, p): self.p = p
        def resolve(self): return self
        @property
        def parent(self): return self
        def __truediv__(self, other): return Path(webchat_dir)
        def exists(self): return True
        
    monkeypatch.setattr(server, "Path", MockPath)
    server.build_app(server.load_config(None))
