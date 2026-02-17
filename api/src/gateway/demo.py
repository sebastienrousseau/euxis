#!/usr/bin/env python3
"""Gateway WebSocket demo client.

Usage:
  python3 gateway/demo.py --url ws://127.0.0.1:18789 --session sess_demo
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os

import websockets


def build_headers() -> dict:
    token = os.environ.get("EUXIS_GATEWAY_TOKEN", "")
    if not token:
        return {}
    return {"Authorization": f"Bearer {token}"}


async def main() -> int:
    parser = argparse.ArgumentParser(description="Gateway demo client")
    parser.add_argument("--url", default="ws://127.0.0.1:18789", help="Gateway WS URL")
    parser.add_argument("--session", default="sess_demo", help="Session ID")
    args = parser.parse_args()

    headers = build_headers()
    async with websockets.connect(args.url, extra_headers=headers) as ws:
        await ws.send(
            json.dumps(
                {
                    "type": "request",
                    "id": "req_connect_1",
                    "method": "gateway.connect",
                    "params": {"protocol": "v0.1", "client_id": "demo"},
                }
            )
        )
        await ws.send(
            json.dumps(
                {
                    "type": "request",
                    "id": "req_demo_1",
                    "method": "chat.send",
                    "params": {
                        "session_id": args.session,
                        "channel_id": "webchat",
                        "role": "user",
                        "content": "Gateway demo run",
                        "meta": {"agent": "architect", "approved": True, "elevated": "full"},
                    },
                }
            )
        )

        async for message in ws:
            print(message)

    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
