#!/usr/bin/env python3
"""Minimal Gateway server skeleton (WS + HTTP health)."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any, Dict

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
import uvicorn

DEFAULT_CONFIG = {
    "gateway": {
        "bind": "127.0.0.1",
        "port": 18789,
        "health_enabled": True,
        "health_path": "/health",
        "auth": {"mode": "token", "token": {"value": ""}, "password": {"value": ""}},
    }
}


class GatewayState:
    def __init__(self) -> None:
        self.started_at = time.time()
        self.sessions_active = 0


STATE = GatewayState()


def load_config(path: Path | None) -> Dict[str, Any]:
    config = json.loads(json.dumps(DEFAULT_CONFIG))
    if path and path.exists():
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception as exc:  # pragma: no cover - config parse error
            print(f"Failed to read config: {exc}", file=sys.stderr)
            return config
        if isinstance(data, dict):
            config = merge_dicts(config, data)
    return config


def merge_dicts(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(base.get(key), dict):
            base[key] = merge_dicts(base[key], value)
        else:
            base[key] = value
    return base


def resolve_config(args: argparse.Namespace) -> Dict[str, Any]:
    config_path = None
    if args.config:
        config_path = Path(args.config).expanduser()
    elif "EUXIS_GATEWAY_CONFIG" in os_environ():
        config_path = Path(os_environ()["EUXIS_GATEWAY_CONFIG"]).expanduser()
    else:
        config_path = Path("~/.euxis/config/gateway.json").expanduser()

    config = load_config(config_path)

    if args.bind:
        config["gateway"]["bind"] = args.bind
    if args.port:
        config["gateway"]["port"] = args.port
    if args.auth_mode:
        config["gateway"]["auth"]["mode"] = args.auth_mode

    return config


def os_environ() -> Dict[str, str]:
    return dict(**{k: v for k, v in dict(**__import__("os").environ).items()})


def build_app(config: Dict[str, Any]) -> FastAPI:
    app = FastAPI()

    health_path = config["gateway"]["health_path"]
    health_enabled = config["gateway"]["health_enabled"]

    @app.get(health_path)  # type: ignore[misc]
    async def health() -> JSONResponse:
        if not health_enabled:
            return JSONResponse({"status": "disabled"}, status_code=404)
        uptime_ms = int((time.time() - STATE.started_at) * 1000)
        return JSONResponse(
            {
                "status": "ok",
                "sessions_active": STATE.sessions_active,
                "uptime_ms": uptime_ms,
            }
        )

    @app.websocket("/")
    async def websocket_endpoint(ws: WebSocket) -> None:
        if not is_authorized(ws, config):
            await ws.close(code=4401)
            return
        await ws.accept()
        try:
            while True:
                message = await ws.receive_text()
                await ws.send_text(
                    json.dumps(
                        {
                            "type": "response",
                            "id": "unknown",
                            "ok": False,
                            "error": {
                                "code": "NOT_IMPLEMENTED",
                                "message": "Gateway skeleton: request handling not implemented",
                            },
                        }
                    )
                )
        except WebSocketDisconnect:
            return

    return app


def is_authorized(ws: WebSocket, config: Dict[str, Any]) -> bool:
    mode = config["gateway"]["auth"].get("mode", "token")
    if mode == "token":
        token = config["gateway"]["auth"].get("token", {}).get("value", "")
        if not token:
            return True
        header = ws.headers.get("authorization") or ""
        return header.strip() == f"Bearer {token}"
    if mode == "password":
        password = config["gateway"]["auth"].get("password", {}).get("value", "")
        if not password:
            return True
        header = ws.headers.get("authorization") or ""
        if not header.lower().startswith("basic "):
            return False
        try:
            import base64
            encoded = header.split(" ", 1)[1].strip()
            decoded = base64.b64decode(encoded).decode("utf-8")
        except Exception:
            return False
        # Expect "username:password" format; only password is checked in v0.1
        if ":" not in decoded:
            return False
        _user, pwd = decoded.split(":", 1)
        return pwd == password
    return True


def cmd_run(args: argparse.Namespace) -> int:
    config = resolve_config(args)
    app = build_app(config)
    uvicorn.run(
        app,
        host=config["gateway"]["bind"],
        port=int(config["gateway"]["port"]),
        log_level="info",
    )
    return 0


def cmd_config(args: argparse.Namespace) -> int:
    config = resolve_config(args)
    print(json.dumps(config, indent=2))
    return 0


def cmd_status(args: argparse.Namespace) -> int:
    config = resolve_config(args)
    bind = config["gateway"]["bind"]
    port = config["gateway"]["port"]
    health_path = config["gateway"]["health_path"]
    url = f"http://{bind}:{port}{health_path}"
    try:
        import httpx

        resp = httpx.get(url, timeout=2.0)
        print(resp.text)
        return 0 if resp.status_code == 200 else 1
    except Exception:
        print("Gateway not responding")
        return 1


def cmd_sessions(args: argparse.Namespace) -> int:
    print("Sessions not implemented in skeleton")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Euxis Gateway CLI")
    subparsers = parser.add_subparsers(dest="command")

    def add_common(sub):
        sub.add_argument("--bind", help="Bind address")
        sub.add_argument("--port", type=int, help="Bind port")
        sub.add_argument("--config", help="Gateway config path")
        sub.add_argument("--auth-mode", help="Override auth mode")

    run_p = subparsers.add_parser("run", help="Run gateway")
    add_common(run_p)

    status_p = subparsers.add_parser("status", help="Gateway status")
    add_common(status_p)

    config_p = subparsers.add_parser("config", help="Print config")
    add_common(config_p)

    sessions_p = subparsers.add_parser("sessions", help="List sessions")
    add_common(sessions_p)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "run":
        return cmd_run(args)
    if args.command == "status":
        return cmd_status(args)
    if args.command == "config":
        return cmd_config(args)
    if args.command == "sessions":
        return cmd_sessions(args)

    parser.print_help()
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
