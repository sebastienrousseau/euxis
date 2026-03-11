import asyncio
import websockets
import json
import uuid
import sys

async def validate():
    url = "ws://127.0.0.2:18789/mcp"
    print(f"Connecting to {url}...")
    
    try:
        async with websockets.connect(url) as ws:
            # 1. Initialize
            print("--- Phase 1: Initialize ---")
            init_id = str(uuid.uuid4())
            await ws.send(json.dumps({
                "jsonrpc": "2.0",
                "id": init_id,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "clientInfo": {"name": "validation-client", "version": "0.1.0"}
                }
            }))
            resp_raw = await ws.recv()
            resp = json.loads(resp_raw)
            print(f"Initialize Response: {json.dumps(resp, indent=2)}")
            
            if "result" not in resp:
                print("Error: Initialization failed")
                return False
                
            await ws.send(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}))

            # 2. List Tools
            print("--- Phase 2: List Tools ---")
            list_id = str(uuid.uuid4())
            await ws.send(json.dumps({
                "jsonrpc": "2.0",
                "id": list_id,
                "method": "tools/list"
            }))
            resp_raw = await ws.recv()
            resp = json.loads(resp_raw)
            tools = resp.get("result", {}).get("tools", [])
            print(f"Tools Found: {[t['name'] for t in tools]}")
            
            tool_names = [t['name'] for t in tools]
            if "get_metrics" not in tool_names:
                print("Error: get_metrics tool not found")
                return False

            # 3. Call Tool (get_metrics)
            print("--- Phase 3: Call Tool (get_metrics) ---")
            call_id = str(uuid.uuid4())
            await ws.send(json.dumps({
                "jsonrpc": "2.0",
                "id": call_id,
                "method": "tools/call",
                "params": {
                    "name": "get_metrics",
                    "arguments": {}
                }
            }))
            resp_raw = await ws.recv()
            resp = json.loads(resp_raw)
            print(f"Call Response: {json.dumps(resp, indent=2)}")
            
            if "result" in resp and "content" in resp["result"]:
                print("MCP Validation Successful!")
                return True
            else:
                print("MCP Validation Failed")
                return False
                
    except Exception as e:
        print(f"Connection failed: {e}")
        return False

if __name__ == "__main__":
    success = asyncio.run(validate())
    sys.exit(0 if success else 1)
