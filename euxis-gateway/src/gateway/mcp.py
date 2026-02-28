# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Model Context Protocol (MCP) Host implementation for Euxis Gateway."""

from __future__ import annotations

import asyncio
import json
import logging
import time
from typing import Any, Dict, List, Optional, Callable, Awaitable

from fastapi import WebSocket, WebSocketDisconnect

logger = logging.getLogger("euxis.gateway.mcp")

# MCP Protocol Version
MCP_VERSION = "2024-11-05"

class MCPHost:
    """MCP Host that manages tools, resources, and client connections."""

    def __init__(self, state: Any, config: Dict[str, Any]):
        self.state = state
        self.config = config
        self.tools: Dict[str, Callable[..., Awaitable[Any]]] = {}
        self.resources: Dict[str, Dict[str, Any]] = {}
        self._setup_default_tools()

    def _setup_default_tools(self):
        """Register built-in Euxis tools as MCP tools."""
        self.register_tool(
            "get_metrics",
            "Retrieve real-time metrics from the Euxis mesh.",
            {
                "type": "object",
                "properties": {
                    "session_id": {"type": "string", "description": "Optional session ID to filter metrics."}
                }
            },
            self._tool_get_metrics
        )
        self.register_tool(
            "sign_payload",
            "Sign a payload using Euxis Cryptographic Provenance.",
            {
                "type": "object",
                "properties": {
                    "payload": {"type": "string", "description": "The payload to sign."},
                    "key_id": {"type": "string", "description": "The cryptographic key ID to use."}
                },
                "required": ["payload"]
            },
            self._tool_sign_payload
        )

    def register_tool(self, name: str, description: str, input_schema: Dict[str, Any], handler: Callable[..., Awaitable[Any]]):
        """Register a new tool with the MCP host."""
        self.tools[name] = {
            "name": name,
            "description": description,
            "inputSchema": input_schema,
            "handler": handler
        }
        logger.info(f"Registered MCP tool: {name}")

    async def handle_connection(self, websocket: WebSocket):
        """Handle an incoming MCP client connection over WebSocket."""
        await websocket.accept()
        client_info = {}
        initialized = False

        try:
            while True:
                message_text = await websocket.receive_text()
                try:
                    request = json.loads(message_text)
                except json.JSONDecodeError:
                    await self._send_error(websocket, None, -32700, "Parse error")
                    continue

                req_id = request.get("id")
                method = request.get("method")
                params = request.get("params", {})

                if not initialized and method != "initialize":
                    await self._send_error(websocket, req_id, -32002, "Not initialized")
                    continue

                if method == "initialize":
                    client_info = params.get("clientInfo", {})
                    initialized = True
                    await self._send_result(websocket, req_id, {
                        "protocolVersion": MCP_VERSION,
                        "capabilities": {
                            "tools": {"listChanged": False},
                            "resources": {"subscribe": True, "listChanged": False}
                        },
                        "serverInfo": {"name": "euxis-gateway", "version": "0.1.0"}
                    })
                elif method == "notifications/initialized":
                    # Client acknowledgment of initialization
                    pass
                elif method == "tools/list":
                    tools_list = [
                        {"name": t["name"], "description": t["description"], "inputSchema": t["inputSchema"]}
                        for t in self.tools.values()
                    ]
                    await self._send_result(websocket, req_id, {"tools": tools_list})
                elif method == "tools/call":
                    tool_name = params.get("name")
                    tool_args = params.get("arguments", {})
                    if tool_name in self.tools:
                        try:
                            result = await self.tools[tool_name]["handler"](**tool_args)
                            await self._send_result(websocket, req_id, {"content": [{"type": "text", "text": json.dumps(result)}]})
                        except Exception as e:
                            await self._send_error(websocket, req_id, -32603, f"Internal error: {str(e)}")
                    else:
                        await self._send_error(websocket, req_id, -32601, f"Tool not found: {tool_name}")
                elif method == "ping":
                    await self._send_result(websocket, req_id, {})
                else:
                    await self._send_error(websocket, req_id, -32601, f"Method not found: {method}")

        except WebSocketDisconnect:
            logger.info("MCP client disconnected")
        except Exception as e:  # pragma: no cover
            logger.error(f"MCP connection error: {str(e)}")

    async def _send_result(self, websocket: WebSocket, req_id: Any, result: Any):
        """Send a JSON-RPC result response."""
        await websocket.send_text(json.dumps({
            "jsonrpc": "2.0",
            "id": req_id,
            "result": result
        }))

    async def _send_error(self, websocket: WebSocket, req_id: Any, code: int, message: str):
        """Send a JSON-RPC error response."""
        await websocket.send_text(json.dumps({
            "jsonrpc": "2.0",
            "id": req_id,
            "error": {"code": code, "message": message}
        }))

    # --- Tool Handlers ---

    async def _tool_get_metrics(self, session_id: Optional[str] = None) -> Dict[str, Any]:
        """Tool handler for retrieving Euxis metrics."""
        # Integration with STATE and euxis-metrics
        return {
            "uptime_ms": int((time.time() - self.state.started_at) * 1000),
            "sessions_active": self.state.sessions_active,
            "session_id": session_id,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        }

    async def _tool_sign_payload(self, payload: str, key_id: Optional[str] = None) -> Dict[str, Any]:
        """Tool handler for signing payloads."""
        # Integration with euxis-crypto-lib
        # For now, a mock implementation that would call into the Rust crypto lib
        import hashlib
        signature = hashlib.sha256(payload.encode()).hexdigest()
        return {
            "payload": payload,
            "signature": f"euxis_sig_{signature}",
            "key_id": key_id or "default_gateway_key",
            "algorithm": "Ed25519"
        }
