#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Minimal Gateway server skeleton (WS + HTTP health)."""

from __future__ import annotations

from pathlib import Path
import sys
from contextlib import asynccontextmanager

import argparse
import asyncio
import base64
import hashlib
import hmac
import json
import os
import shlex
import time
from urllib import error as urlerror
from urllib import request as urlrequest
from typing import Any, Dict, List, Optional, Tuple
import logging

# SECURITY: Configure logging without exposing sensitive data (S4-004 fix)
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%dT%H:%M:%S",
)
logger = logging.getLogger("euxis.gateway")

# SECURITY: Standardized timeout configuration (S4-005 fix)
TIMEOUT_CONFIG = {
    "webhook": 5,       # External webhook calls
    "health_check": 2,  # Quick health probes
    "api_call": 10,     # Internal API calls
    "tts_command": 30,  # TTS generation (can be slow)
}


# PERFORMANCE: Pre-computed paths - avoid repeated path resolution
# Computed once at module load, cached for all subsequent imports
_PATHS_INITIALIZED = False
_REPO_ROOT: Path | None = None


def _ensure_repo_paths() -> None:
    """Initialize repository paths once and cache them.

    Performance optimization: path resolution is expensive (~5-10ms).
    By caching, we reduce cold start overhead significantly.
    """
    global _PATHS_INITIALIZED, _REPO_ROOT

    if _PATHS_INITIALIZED:
        return

    _REPO_ROOT = Path(__file__).resolve().parents[3]
    module_src_dirs = [
        "api/src",
        "packages/shared/src",
    ]

    repo_root_str = str(_REPO_ROOT)
    if repo_root_str not in sys.path:
        sys.path.insert(0, repo_root_str)

    for rel in module_src_dirs:
        path = _REPO_ROOT / rel
        if path.exists():
            path_str = str(path)
            if path_str not in sys.path:
                sys.path.insert(0, path_str)

    _PATHS_INITIALIZED = True


_ensure_repo_paths()

from fastapi import FastAPI, APIRouter, File, Request, UploadFile, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

from shared.adapter_loader import build_adapters
from shared.gateway_utils import (
    load_session_from_disk,
    load_session_meta,
    load_run_events,
    load_approvals,
    load_cron_jobs,
    load_canvas_state,
    load_webhooks,
    persist_approval,
    persist_cron_jobs,
    persist_webhooks,
    delete_approval,
    audit_log,
    persist_transcript,
    persist_message,
    persist_run_event,
    persist_session_meta,
    persist_canvas_state,
    persist_voice_blob,
    persist_voice_text,
    append_voice_chunk,
    resolve_voice_blob,
    cleanup_voice,
    runs_dir,
    identities_dir,
    make_session_id,
    timestamp,
)
from .mcp import MCPHost
from .identity import IdentityManager


DEFAULT_CONFIG = {
    
    "gateway": {
        "bind": "127.0.0.2",
        "port": 18789,
        "health_enabled": True,
        "health_path": "/health",
        "auth": {"mode": "token", "token": {"value": ""}, "password": {"value": ""}},
        "exec": {
            "policy": "allowlist",
            "ask": "on-miss",
            "ask_fallback": "deny",
            "allowlist": [],
        },
        "channels": {
            "slack": {"enabled": False, "mode": "socket", "token": "", "app_token": "", "signing_secret": ""},
            "telegram": {"enabled": False, "mode": "webhook", "token": "", "webhook_url": ""},
            "discord": {"enabled": False, "token": ""},
            "whatsapp": {"enabled": False, "token": "", "phone_number_id": "", "verify_token": ""},
        },
        "policy": {"mention_required": True, "allowlist": []},
        "mcp_enabled": True,
    }
}


class GatewayState:
    def __init__(self) -> None:
        self.started_at = time.time()
        self.sessions_active = 0
        self.sessions: Dict[str, List[Dict[str, Any]]] = {}
        self.session_version = 0
        self.last_event_ts = ""
        self.running: Dict[str, asyncio.Task] = {}
        self.pending_approvals: Dict[str, Dict[str, Any]] = {}
        self.adapters: Dict[str, Any] = {}
        self.connections: Dict[str, WebSocket] = {}
        self.conn_seq: Dict[str, int] = {}
        self.conn_sessions: Dict[str, str] = {}
        self.session_locks: Dict[str, asyncio.Lock] = {}
        self.cron_jobs: List[Dict[str, Any]] = []
        self.cron_task: Optional[asyncio.Task] = None
        self.config: Dict[str, Any] = {}
        self.webhooks: List[Dict[str, Any]] = []
        self.voice_connections: Dict[str, List[WebSocket]] = {}
        self.mcp_host: Optional[Any] = None
        self.identity_manager: IdentityManager = IdentityManager(identities_dir())
        self.device_nodes: Dict[str, Dict[str, Any]] = {}


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
        else:
            print("Gateway config must be a JSON object; ignoring invalid payload.", file=sys.stderr)
    elif path:
        print(f"Gateway config not found at {path}; using defaults.", file=sys.stderr)
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
        config_path = Path("~/.euxis/security/gateway.json").expanduser()

    config = load_config(config_path)

    if args.bind:
        config["gateway"]["bind"] = args.bind
    if args.port:
        config["gateway"]["port"] = args.port
    if args.auth_mode:
        config["gateway"]["auth"]["mode"] = args.auth_mode

    config["gateway"].setdefault("channels", {})
    return config


def os_environ() -> Dict[str, str]:
    return dict(**os.environ)


def verify_slack_signature(request: Request, body: bytes, signing_secret: str) -> bool:
    timestamp = request.headers.get("X-Slack-Request-Timestamp", "")
    signature = request.headers.get("X-Slack-Signature", "")
    if not timestamp or not signature:
        return False
    try:
        ts_value = int(timestamp)
    except ValueError:
        return False
    if abs(time.time() - ts_value) > 300:
        return False
    base = f"v0:{timestamp}:".encode("utf-8") + body
    digest = hmac.new(signing_secret.encode("utf-8"), base, hashlib.sha256).hexdigest()
    expected = f"v0={digest}"
    return hmac.compare_digest(expected, signature)


# SECURITY: Audio file magic byte signatures for content validation
AUDIO_MAGIC_BYTES = {
    "wav": [(0, b"RIFF"), (8, b"WAVE")],  # RIFF....WAVE
    "mp3": [(0, b"\xff\xfb"), (0, b"\xff\xfa"), (0, b"\xff\xf3"), (0, b"\xff\xf2"), (0, b"ID3")],
    "ogg": [(0, b"OggS")],
    "flac": [(0, b"fLaC")],
    "m4a": [(4, b"ftyp")],  # ....ftypM4A or similar
    "aac": [(0, b"\xff\xf1"), (0, b"\xff\xf9")],
    "webm": [(0, b"\x1a\x45\xdf\xa3")],  # EBML header
}


def _validate_audio_magic(blob: bytes, extension: str) -> bool:
    """SECURITY: Validate audio file content matches expected magic bytes."""
    if len(blob) < 12:
        return False
    signatures = AUDIO_MAGIC_BYTES.get(extension, [])
    if not signatures:
        # Unknown extension - reject
        return False
    for offset, magic in signatures:
        if len(blob) > offset + len(magic):
            if blob[offset : offset + len(magic)] == magic:
                return True
    return False


from collections import defaultdict
from fastapi.middleware.cors import CORSMiddleware

# SECURITY: Rate limiting state (S3-001 fix)
RATE_LIMIT_WINDOW = 60  # seconds
RATE_LIMIT_MAX_REQUESTS = 100  # per window
RATE_LIMIT_AUTH_FAILURES = 5  # max auth failures before lockout
_rate_limit_state: Dict[str, List[float]] = defaultdict(list)
_auth_failure_state: Dict[str, List[float]] = defaultdict(list)


def _check_rate_limit(client_ip: str) -> bool:
    """SECURITY: Check if client is within rate limits."""
    now = time.time()
    # Clean old entries
    _rate_limit_state[client_ip] = [t for t in _rate_limit_state[client_ip] if now - t < RATE_LIMIT_WINDOW]
    if len(_rate_limit_state[client_ip]) >= RATE_LIMIT_MAX_REQUESTS:
        return False
    _rate_limit_state[client_ip].append(now)
    return True


def _check_auth_lockout(client_ip: str) -> bool:
    """SECURITY: Check if client is locked out due to auth failures."""
    now = time.time()
    _auth_failure_state[client_ip] = [t for t in _auth_failure_state[client_ip] if now - t < RATE_LIMIT_WINDOW * 5]
    return len(_auth_failure_state[client_ip]) < RATE_LIMIT_AUTH_FAILURES


def _record_auth_failure(client_ip: str) -> None:
    """SECURITY: Record an authentication failure."""
    _auth_failure_state[client_ip].append(time.time())


def build_app(config: Dict[str, Any]) -> FastAPI:
    @asynccontextmanager
    async def lifespan(_: FastAPI):
        STATE.cron_jobs = load_cron_jobs()
        persisted_hooks = load_webhooks()
        if persisted_hooks:
            STATE.webhooks = persisted_hooks
        else:
            STATE.webhooks = config["gateway"].get("webhooks", [])
        STATE.cron_task = asyncio.create_task(cron_loop(config))
        for name, adapter in STATE.adapters.items():
            try:
                adapter.connect()
            except Exception:  # pragma: no cover - best effort
                # SECURITY: Don't expose exception details in logs (S4-004 fix)
                logger.warning("Adapter %s connect failed", name)
        try:
            yield
        finally:
            if STATE.cron_task:
                STATE.cron_task.cancel()
            for name, adapter in STATE.adapters.items():
                try:
                    adapter.disconnect()
                except Exception:  # pragma: no cover - best effort
                    # SECURITY: Don't expose exception details in logs (S4-004 fix)
                    logger.warning("Adapter %s disconnect failed", name)

    app = FastAPI(lifespan=lifespan)
    STATE.config = config

    # SECURITY: Add CORS middleware (S3-003 fix)
    cors_origins = config["gateway"].get("cors", {}).get("origins", ["http://localhost:*"])
    app.add_middleware(
        CORSMiddleware,
        allow_origins=cors_origins,
        allow_credentials=True,
        allow_methods=["GET", "POST", "DELETE"],
        allow_headers=["Authorization", "Content-Type", "X-Admin-Token", "X-CSRF-Token"],
    )

    # SECURITY: Add security headers middleware (S3-003 fix)
    @app.middleware("http")
    async def add_security_headers(request: Request, call_next):
        # Rate limiting check (S3-001)
        client_ip = request.client.host if request.client else "unknown"
        if not _check_rate_limit(client_ip):
            return JSONResponse(
                {"status": "rate_limited", "retry_after": RATE_LIMIT_WINDOW},
                status_code=429,
                headers={"Retry-After": str(RATE_LIMIT_WINDOW)}
            )
        response = await call_next(request)
        # Security headers
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-XSS-Protection"] = "1; mode=block"
        response.headers["Referrer-Policy"] = "strict-origin-when-cross-origin"
        response.headers["Cache-Control"] = "no-store"
        if config["gateway"].get("https_only", False):
            response.headers["Strict-Transport-Security"] = "max-age=31536000; includeSubDomains"
        return response
    session_cfg = config["gateway"].setdefault("session", {})
    session_cfg.setdefault("dm_scope", "main")
    session_cfg.setdefault("account_id", "default")

    def _adapter_handler(session_id: str, content: str, meta: Dict[str, Any]) -> None:
        channel_id = meta.get("channel_id", "unknown")
        chat_id = str(meta.get("chat_id") or meta.get("channel_id") or "unknown")
        thread_id = meta.get("thread_id") or meta.get("thread_ts")
        sender = meta.get("sender")
        resolved_id = make_session_id(
            channel_id,
            chat_id,
            str(thread_id) if thread_id else None,
            session_cfg.get("dm_scope", "main"),
            session_cfg.get("account_id", "default"),
            str(sender) if sender else None,
        )
        normalized_meta = dict(meta)
        normalized_meta.setdefault("channel_id", channel_id)
        normalized_meta.setdefault("scope", "dm")
        normalized_meta.setdefault("agent", "architect")
        normalized_meta.setdefault("approved", True)
        asyncio.create_task(process_inbound(resolved_id, content, normalized_meta, config))

    STATE.adapters = build_adapters(config, on_message=_adapter_handler)
    STATE.pending_approvals = load_approvals()

    if config["gateway"].get("mcp_enabled", True):
        STATE.mcp_host = MCPHost(STATE, config)

    slack_cfg = config["gateway"].get("channels", {}).get("slack", {})
    telegram_cfg = config["gateway"].get("channels", {}).get("telegram", {})

    health_path = config["gateway"]["health_path"]
    health_enabled = config["gateway"]["health_enabled"]

    @app.get(health_path)  # type: ignore[misc]
    async def health(request: Request) -> JSONResponse:
        if not health_enabled:
            return JSONResponse({"status": "disabled"}, status_code=404)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        uptime_ms = int((time.time() - STATE.started_at) * 1000)
        return JSONResponse(
            {
                "status": "ok",
                "sessions_active": STATE.sessions_active,
                "last_event_ts": STATE.last_event_ts or None,
                "uptime_ms": uptime_ms,
            }
        )


    orchestrator_router = APIRouter()
    auth_router = APIRouter()
    mesh_router = APIRouter()

    deps = dict(globals())
    deps.update(locals())

    from .routers.orchestrator import setup_orchestrator
    from .routers.auth import setup_auth
    from .routers.mesh import setup_mesh

    setup_orchestrator(orchestrator_router, deps)
    setup_auth(auth_router, deps)
    setup_mesh(mesh_router, deps)

    app.include_router(orchestrator_router)
    app.include_router(auth_router)
    app.include_router(mesh_router)
    @app.websocket("/")
    async def websocket_endpoint(ws: WebSocket) -> None:
        allowed, reason = is_authorized(ws, config)
        if not allowed:
            await ws.accept()
            await ws.send_text(
                json.dumps(
                    {
                        "type": "response",
                        "id": "auth",
                        "ok": False,
                        "error": {"code": "UNAUTHORIZED", "message": reason or "unauthorized"},
                    }
                )
            )
            await ws.close(code=4401)
            return
        await ws.accept()
        conn_id = f"conn_{int(time.time() * 1000)}"
        STATE.connections[conn_id] = ws
        STATE.conn_seq[conn_id] = 0
        STATE.conn_sessions[conn_id] = ""
        seq_state = {"value": 0}
        tick_task = asyncio.create_task(send_ticks(ws, seq_state))
        await send_presence(ws, seq_state)
        try:
            while True:
                message = await ws.receive_text()
                await handle_frame(ws, message, seq_state, config, conn_id)
        except WebSocketDisconnect:
            tick_task.cancel()
            STATE.connections.pop(conn_id, None)
            STATE.conn_seq.pop(conn_id, None)
            STATE.conn_sessions.pop(conn_id, None)
            return

    @app.websocket("/voice/stream")
    async def voice_stream(ws: WebSocket) -> None:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            await ws.accept()
            await ws.send_text(json.dumps({"type": "error", "error": "voice_disabled"}))
            await ws.close(code=4404)
            return
        await ws.accept()
        session_id: Optional[str] = None
        meta: Dict[str, Any] = {}
        seen_audio = False
        try:
            while True:
                message = await ws.receive()
                if "bytes" in message and message["bytes"] is not None:
                    if not session_id:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_session_id"}))
                        continue
                    append_voice_chunk(session_id, message["bytes"])
                    seen_audio = True
                    await ws.send_text(json.dumps({"type": "ack", "event": "chunk"}))
                    continue
                if "text" not in message:
                    continue
                try:
                    payload = json.loads(message["text"])
                except Exception:
                    await ws.send_text(json.dumps({"type": "error", "error": "invalid_json"}))
                    continue
                event = payload.get("event")
                if event == "start":
                    session_id = payload.get("session_id")
                    meta = payload.get("meta", {}) if isinstance(payload.get("meta"), dict) else {}
                    if not session_id:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_session_id"}))
                        continue
                    STATE.voice_connections.setdefault(session_id, []).append(ws)
                    await ws.send_text(json.dumps({"type": "ack", "event": "start"}))
                    continue
                if event == "chunk":
                    session_id = payload.get("session_id") or session_id
                    if not session_id:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_session_id"}))
                        continue
                    encoded = payload.get("data", "")
                    if not encoded:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_data"}))
                        continue
                    try:
                        chunk = base64.b64decode(encoded)
                    except Exception:
                        await ws.send_text(json.dumps({"type": "error", "error": "invalid_base64"}))
                        continue
                    append_voice_chunk(session_id, chunk, payload.get("format", "raw"))
                    seen_audio = True
                    await ws.send_text(json.dumps({"type": "ack", "event": "chunk"}))
                    continue
                if event == "stt":
                    session_id = payload.get("session_id") or session_id
                    transcript = payload.get("content", "")
                    if not session_id or not transcript:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_content"}))
                        continue
                    meta = payload.get("meta", meta) if isinstance(payload.get("meta"), dict) else meta
                    persist_voice_text(
                        session_id,
                        {"ts": timestamp(), "event": "stt", "content": transcript, "meta": meta},
                    )
                    await ws.send_text(json.dumps({"type": "ack", "event": "stt"}))
                    continue
                if event == "tts":
                    session_id = payload.get("session_id") or session_id
                    text = payload.get("content", "")
                    if not session_id or not text:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_content"}))
                        continue
                    persist_voice_text(
                        session_id,
                        {"ts": timestamp(), "event": "tts", "content": text, "meta": meta},
                    )
                    tts_cfg = config["gateway"].get("voice", {}).get("tts", {})
                    if tts_cfg.get("mode") == "webhook" and tts_cfg.get("webhook_url"):
                        await post_json(
                            tts_cfg["webhook_url"],
                            {"session_id": session_id, "text": text, "meta": meta},
                        )
                    await ws.send_text(json.dumps({"type": "ack", "event": "tts"}))
                    continue
                if event == "text":
                    session_id = payload.get("session_id") or session_id
                    content = payload.get("content", "")
                    if not session_id or not content:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_content"}))
                        continue
                    meta = payload.get("meta", meta) if isinstance(payload.get("meta"), dict) else meta
                    meta.setdefault("agent", "orchestrator")
                    meta.setdefault("channel_id", "voice")
                    meta.setdefault("scope", "dm")
                    meta.setdefault("approved", True)
                    persist_voice_text(
                        session_id,
                        {"ts": timestamp(), "event": "text", "content": content, "meta": meta},
                    )
                    asyncio.create_task(process_inbound(session_id, content, meta, config))
                    await ws.send_text(json.dumps({"type": "ack", "event": "text"}))
                    continue
                if event == "end":
                    if seen_audio:
                        stt_cfg = config["gateway"].get("voice", {}).get("stt", {})
                        if stt_cfg.get("mode") == "webhook" and stt_cfg.get("webhook_url"):
                            audio_path = resolve_voice_blob(session_id, payload.get("format", "raw"))
                            await post_json(
                                stt_cfg["webhook_url"],
                                {
                                    "session_id": session_id,
                                    "meta": meta,
                                    "audio_path": str(audio_path) if audio_path else None,
                                },
                            )
                        if stt_cfg.get("mode") == "command" and stt_cfg.get("command"):
                            audio_path = resolve_voice_blob(session_id, payload.get("format", "raw"))
                            transcript = await run_voice_command(
                                stt_cfg.get("command", ""),
                                {"audio_path": str(audio_path) if audio_path else ""},
                            )
                            if transcript:
                                persist_voice_text(
                                    session_id,
                                    {"ts": timestamp(), "event": "stt", "content": transcript, "meta": meta},
                                )
                                auto_inject = bool(stt_cfg.get("auto_inject", True))
                                if auto_inject:
                                    meta.setdefault("agent", "orchestrator")
                                    meta.setdefault("channel_id", "voice")
                                    meta.setdefault("scope", "dm")
                                    meta.setdefault("approved", True)
                                    asyncio.create_task(process_inbound(session_id, transcript, meta, config))
                    await ws.send_text(json.dumps({"type": "ack", "event": "end"}))
                    continue
                await ws.send_text(json.dumps({"type": "error", "error": "unknown_event"}))
        except WebSocketDisconnect:
            if session_id and session_id in STATE.voice_connections:
                STATE.voice_connections[session_id] = [
                    conn for conn in STATE.voice_connections[session_id] if conn is not ws
                ]
                if not STATE.voice_connections[session_id]:
                    STATE.voice_connections.pop(session_id, None)
            return

    @app.websocket("/mcp")
    async def mcp_endpoint(ws: WebSocket) -> None:
        if not config["gateway"].get("mcp_enabled", True):
            await ws.accept()
            await ws.send_text(json.dumps({"type": "error", "error": "mcp_disabled"}))
            await ws.close(code=4404)
            return
        allowed, reason = is_authorized(ws, config)
        if not allowed:
            await ws.accept()
            await ws.send_text(
                json.dumps(
                    {
                        "jsonrpc": "2.0",
                        "error": {"code": -32001, "message": reason or "unauthorized"},
                    }
                )
            )
            await ws.close(code=4401)
            return
        if STATE.mcp_host:
            await STATE.mcp_host.handle_connection(ws)

    webchat_dir = Path(__file__).resolve().parent / "webchat"
    if webchat_dir.exists():  # pragma: no cover
        app.mount("/webchat", StaticFiles(directory=str(webchat_dir), html=True), name="webchat")  # pragma: no cover

    return app


def is_authorized(ws: WebSocket, config: Dict[str, Any]) -> Tuple[bool, Optional[str]]:
    mode = config["gateway"]["auth"].get("mode", "token")
    if mode == "token":
        token = config["gateway"]["auth"].get("token", {}).get("value", "")
        if not token:
            # SECURITY: Empty token = DENY access (require explicit token configuration)
            return False, "auth_not_configured"
        header = ws.headers.get("authorization") or ""
        if header.strip() == f"Bearer {token}":
            return True, None
        # SECURITY: Query param auth is deprecated and disabled by default (S2-003 fix)
        # Tokens in URLs are logged, cached, and exposed in Referer headers
        allow_query = config["gateway"]["auth"].get("token", {}).get("allow_query_param", False)
        if allow_query:
            print("[SECURITY WARNING] Query parameter authentication is insecure and deprecated", file=sys.stderr)
            query_token = ws.query_params.get("token", "")
            if query_token == token:
                audit_log({"ts": timestamp(), "event": "auth_query_param_used", "warning": "insecure_method"})
                return True, None
        return False, "invalid_token"
    if mode == "password":
        password = config["gateway"]["auth"].get("password", {}).get("value", "")
        if not password:
            # SECURITY: Empty password = DENY access (require explicit password configuration)
            return False, "auth_not_configured"
        header = ws.headers.get("authorization") or ""
        if not header.lower().startswith("basic "):
            return False, "missing_basic"
        try:
            import base64
            encoded = header.split(" ", 1)[1].strip()
            decoded = base64.b64decode(encoded).decode("utf-8")
        except Exception:
            return False, "invalid_basic"
        # Expect "username:password" format; only password is checked in v0.1
        if ":" not in decoded:
            return False, "invalid_basic"
        _user, pwd = decoded.split(":", 1)
        return (pwd == password), None
    if mode == "none":
        # Explicit opt-in to no authentication (must be configured deliberately)
        return True, None
    return False, "auth_not_configured"


def is_http_authorized(request: Request, config: Dict[str, Any]) -> Tuple[bool, Optional[str]]:
    mode = config["gateway"]["auth"].get("mode", "token")
    if mode == "token":
        token = config["gateway"]["auth"].get("token", {}).get("value", "")
        if not token:
            # SECURITY: Empty token = DENY access (require explicit token configuration)
            return False, "auth_not_configured"
        header = request.headers.get("authorization") or ""
        if header.strip() == f"Bearer {token}":
            return True, None
        # SECURITY: Query param auth is deprecated and disabled by default (S2-003 fix)
        allow_query = config["gateway"]["auth"].get("token", {}).get("allow_query_param", False)
        if allow_query:
            print("[SECURITY WARNING] Query parameter authentication is insecure and deprecated", file=sys.stderr)
            query_token = request.query_params.get("token", "")
            if query_token == token:
                audit_log({"ts": timestamp(), "event": "auth_query_param_used", "warning": "insecure_method"})
                return True, None
        return False, "invalid_token"
    if mode == "password":
        password = config["gateway"]["auth"].get("password", {}).get("value", "")
        if not password:
            # SECURITY: Empty password = DENY access (require explicit password configuration)
            return False, "auth_not_configured"
        header = request.headers.get("authorization") or ""
        if not header.lower().startswith("basic "):
            return False, "missing_basic"
        try:
            import base64
            encoded = header.split(" ", 1)[1].strip()
            decoded = base64.b64decode(encoded).decode("utf-8")
        except Exception:
            return False, "invalid_basic"
        if ":" not in decoded:
            return False, "invalid_basic"
        _user, pwd = decoded.split(":", 1)
        return (pwd == password), None
    if mode == "none":
        # Explicit opt-in to no authentication (must be configured deliberately)
        return True, None
    return False, "auth_not_configured"


def ensure_session(session_id: str) -> None:
    if session_id not in STATE.sessions:
        STATE.sessions[session_id] = []
        STATE.sessions_active = len(STATE.sessions)
        STATE.session_version += 1
    if session_id not in STATE.session_locks:
        STATE.session_locks[session_id] = asyncio.Lock()


def append_message(session_id: str, role: str, content: str) -> Dict[str, Any]:
    entry = {
        "message_id": f"msg_{int(time.time() * 1000)}",
        "role": role,
        "content": content,
        "timestamp": timestamp(),
    }
    STATE.sessions[session_id].append(entry)
    persist_message(session_id, entry)
    return entry


async def handle_frame(
    ws: WebSocket,
    raw: str,
    seq_state: Dict[str, int],
    config: Dict[str, Any],
    conn_id: str,
) -> None:
    try:
        frame = json.loads(raw)
    except Exception:
        await ws.send_text(
            json.dumps(
                {
                    "type": "response",
                    "id": "unknown",
                    "ok": False,
                    "error": {"code": "INVALID_REQUEST", "message": "Invalid JSON"},
                }
            )
        )
        return

    if frame.get("type") == "event":
        event_name = frame.get("event")
        if event_name == "node.browser.stream_frame":
            # Broadcast the frame to all connected TUI/WebChat clients
            payload = json.dumps(frame)
            for c_ws in STATE.connections.values():
                if c_ws != ws:  # Don't send back to the node
                    try:
                        await c_ws.send_text(payload)
                    except Exception:
                        pass
        return

    if frame.get("type") != "request":
        await ws.send_text(
            json.dumps(
                {
                    "type": "response",
                    "id": frame.get("id", "unknown"),
                    "ok": False,
                    "error": {"code": "INVALID_REQUEST", "message": "Expected request frame"},
                }
            )
        )
        return

    req_id = frame.get("id", "unknown")
    method = frame.get("method", "")
    params = frame.get("params", {}) if isinstance(frame.get("params"), dict) else {}

    if method == "node.register":
        node_id = f"node_{conn_id}"
        STATE.device_nodes[node_id] = {  # pragma: no cover
            "conn_id": conn_id,  # pragma: no cover
            "ws": ws,  # pragma: no cover
            "hostname": params.get("hostname"),  # pragma: no cover
            "platform": params.get("platform"),  # pragma: no cover
            "capabilities": params.get("capabilities", [])  # pragma: no cover
        }
        await send_result(ws, req_id, {"node_id": node_id})  # pragma: no cover
        return

    if method.startswith("node."):
        # Route to first available node for now
        if not STATE.device_nodes:
            await send_error(ws, req_id, "NODE_UNAVAILABLE", "No device nodes connected")
            return
        node_id = next(iter(STATE.device_nodes))
        node_ws = STATE.device_nodes[node_id]["ws"]
        await node_ws.send_text(raw)
        return

    if method == "gateway.connect":
        protocol = params.get("protocol", "")
        if not protocol:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing protocol")
            return
        result = {
            "protocol": "v0.1",
            "server_time": timestamp(),
            "stateVersion": STATE.session_version,
            "sessions_active": STATE.sessions_active,
        }
        await send_result(ws, req_id, result)
        await send_presence(ws, seq_state)
        return

    if method == "chat.history":
        session_id = params.get("session_id", "")
        if not session_id:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing session_id")
            return
        ensure_session(session_id)
        STATE.conn_sessions[conn_id] = session_id
        if not STATE.sessions[session_id]:
            STATE.sessions[session_id] = load_session_from_disk(session_id)
        limit = int(params.get("limit", 200))
        messages = STATE.sessions[session_id][-limit:]
        await send_result(ws, req_id, {"messages": messages})
        return

    if method == "chat.send":
        session_id = params.get("session_id", "")
        role = params.get("role", "")
        content = params.get("content", "")
        meta = params.get("meta", {}) if isinstance(params.get("meta"), dict) else {}
        if not session_id or not role:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing session_id or role")
            return
        ensure_session(session_id)
        STATE.conn_sessions[conn_id] = session_id
        entry = append_message(session_id, role, content)
        session_meta = load_session_meta(session_id)
        session_meta.update(
            {
                "session_id": session_id,
                "channel_id": params.get("channel_id", "webchat"),
                "scope": params.get("scope", "dm"),
                "owner": meta.get("agent", "architect"),
                "updated_at": timestamp(),
            }
        )
        persist_session_meta(session_id, session_meta)
        run_id = f"run_{int(time.time() * 1000)}"
        if not is_message_allowed(meta, config):
            await send_error(ws, req_id, "POLICY_BLOCKED", "Message blocked by policy")
            audit_log({"ts": timestamp(), "event": "policy_block", "session_id": session_id})
            return
        if not is_exec_allowed(meta, config):
            pending = {
                "run_id": run_id,
                "session_id": session_id,
                "agent_id": meta.get("agent", "architect"),
                "requested_at": timestamp(),
                "meta": meta,
                "content": content,
                "approved": False,
            }
            STATE.pending_approvals[run_id] = pending
            persist_approval(run_id, pending)
            audit_log({"ts": timestamp(), "event": "approval_requested", "run_id": run_id})
            await send_error(ws, req_id, "APPROVAL_REQUIRED", "Execution approval required")
            return
        await send_result(ws, req_id, {"message_id": entry["message_id"], "session_id": session_id, "run_id": run_id})
        task = asyncio.create_task(
            dispatch_with_lock(ws, seq_state, session_id, run_id, meta, content, config)
        )
        STATE.running[run_id] = task
        return

    if method == "chat.inject":
        session_id = params.get("session_id", "")
        role = params.get("role", "")
        content = params.get("content", "")
        if not session_id or not role:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing session_id or role")
            return
        ensure_session(session_id)
        STATE.conn_sessions[conn_id] = session_id
        entry = append_message(session_id, role, content)
        await send_result(ws, req_id, {"message_id": entry["message_id"], "session_id": session_id})
        return

    if method == "chat.abort":
        run_id = params.get("run_id", "")
        task = STATE.running.get(run_id)
        if task:
            task.cancel()
            STATE.running.pop(run_id, None)
        await send_result(ws, req_id, {"aborted": True})
        return

    await send_error(ws, req_id, "INVALID_REQUEST", "Unknown method")


async def dispatch_agent(
    ws: WebSocket,
    seq_state: Dict[str, int],
    session_id: str,
    run_id: str,
    meta: Dict[str, Any],
    content: str,
    config: Dict[str, Any],
) -> None:
    agent_id = meta.get("agent", "architect")
    provider = meta.get("provider")
    mode = meta.get("mode", "agent")
    if mode == "squad":
        squad = meta.get("squad", "build")
        cmd = [str(Path.home() / ".euxis" / "bin" / "euxis-squad"), "deploy", squad, content]
    elif mode == "combo":
        combo = meta.get("combo", "envision")
        cmd = [str(Path.home() / ".euxis" / "bin" / "euxis-combo"), "run", combo, content]
    elif mode == "playbook":
        playbook = meta.get("playbook", "zero-to-one")
        cmd = [str(Path.home() / ".euxis" / "bin" / "euxis-playbook"), "run", playbook, content]
    elif mode == "wasm":
        wasm_plugin = meta.get("plugin", agent_id)
        cmd = [str(Path.home() / ".euxis" / "bin" / "euxis-wasm"), wasm_plugin, content]
        if config["gateway"].get("mcp_enabled", True):
            bind = config["gateway"]["bind"]
            port = config["gateway"]["port"]
            cmd.extend(["--mcp-url", f"ws://{bind}:{port}/mcp"])
            token = config["gateway"]["auth"].get("token", {}).get("value", "")
            if token:  # pragma: no cover
                cmd.extend(["--mcp-token", token])
    else:
        cmd = [str(Path.home() / ".euxis" / "bin" / "euxis"), agent_id, content]
    if provider:
        cmd.append(provider)

    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
    except Exception as exc:
        await send_agent_event(ws, seq_state, run_id, session_id, agent_id, "error", str(exc))
        return

    output_lines: List[str] = []

    async def stream_stdout() -> None:
        assert proc.stdout
        async for line in proc.stdout:
            text = line.decode("utf-8", errors="ignore").rstrip()
            if not text:
                continue
            output_lines.append(text)
            await send_agent_event(ws, seq_state, run_id, session_id, agent_id, "stream", text)

    async def stream_stderr() -> None:
        assert proc.stderr
        async for line in proc.stderr:
            text = line.decode("utf-8", errors="ignore").rstrip()
            if not text:
                continue
            output_lines.append(text)
            await send_agent_event(ws, seq_state, run_id, session_id, agent_id, "stream", text)

    async with asyncio.TaskGroup() as tg:
        tg.create_task(stream_stdout())
        tg.create_task(stream_stderr())
    await proc.wait()
    status = "final" if proc.returncode == 0 else "error"
    final_content = "\n".join(output_lines[-200:]) if output_lines else f"exit {proc.returncode}"
    await send_agent_event(ws, seq_state, run_id, session_id, agent_id, status, final_content)
    STATE.running.pop(run_id, None)


async def dispatch_with_lock(
    ws: WebSocket,
    seq_state: Dict[str, int],
    session_id: str,
    run_id: str,
    meta: Dict[str, Any],
    content: str,
    config: Dict[str, Any],
) -> None:
    lock = STATE.session_locks.setdefault(session_id, asyncio.Lock())
    async with lock:
        await dispatch_agent(ws, seq_state, session_id, run_id, meta, content, config)


async def send_ticks(ws: WebSocket, seq_state: Dict[str, int]) -> None:
    try:
        while True:
            await asyncio.sleep(30)
            seq_state["value"] += 1
            payload = {
                "type": "event",
                "event": "tick",
                "seq": seq_state["value"],
                "ts": timestamp(),
                "data": {"uptime_ms": int((time.time() - STATE.started_at) * 1000), "stateVersion": STATE.session_version},
            }
            STATE.last_event_ts = payload["ts"]
            await ws.send_text(json.dumps(payload))
    except asyncio.CancelledError:
        return


async def cron_loop(config: Dict[str, Any]) -> None:
    try:
        last_cleanup = 0.0
        while True:
            now = time.time()
            changed = False
            for job in STATE.cron_jobs:
                every = int(job.get("every_seconds", 0))
                if every <= 0:
                    continue
                last_run = float(job.get("last_run", 0))
                if now - last_run < every:
                    continue
                job["last_run"] = now
                changed = True
                meta = job.get("meta", {})
                session_id = job.get("session_id", "")
                content = job.get("content", "")
                if not session_id or not content:
                    continue
                asyncio.create_task(process_inbound(session_id, content, meta, config))
            if changed:
                persist_cron_jobs(STATE.cron_jobs)
            if now - last_cleanup > 3600:
                retention = int(config["gateway"].get("voice", {}).get("retention_hours", 24))
                cleanup_voice(retention)
                last_cleanup = now
            await asyncio.sleep(1)
    except asyncio.CancelledError:
        return


async def send_presence(ws: WebSocket, seq_state: Dict[str, int]) -> None:
    seq_state["value"] += 1
    payload = {
        "type": "event",
        "event": "presence",
        "seq": seq_state["value"],
        "ts": timestamp(),
        "data": {"stateVersion": STATE.session_version, "sessions_active": STATE.sessions_active},
    }
    STATE.last_event_ts = payload["ts"]
    await ws.send_text(json.dumps(payload))


async def send_agent_event(
    ws: WebSocket,
    seq_state: Dict[str, int],
    run_id: str,
    session_id: str,
    agent_id: str,
    status: str,
    content: str,
) -> None:
    seq_state["value"] += 1
    payload = {
        "type": "event",
        "event": "agent",
        "seq": seq_state["value"],
        "ts": timestamp(),
        "data": {
            "run_id": run_id,
            "session_id": session_id,
            "agent_id": agent_id,
            "status": status,
            "content": content,
        },
    }
    STATE.last_event_ts = payload["ts"]
    persist_run_event(run_id, payload)
    await ws.send_text(json.dumps(payload))
    if status in {"final", "error"}:
        await notify_webhooks(payload)
        await deliver_to_channel(session_id, content)
        await push_voice_tts(session_id, content)


async def notify_webhooks(payload: Dict[str, Any]) -> None:
    hooks = STATE.webhooks or STATE.config.get("gateway", {}).get("webhooks", [])
    if not hooks:
        return
    event_name = f"{payload.get('event')}.{payload.get('data', {}).get('status', '')}"
    body = json.dumps(payload).encode("utf-8")
    for hook in hooks:
        url = hook.get("url")
        events = hook.get("events", ["agent.final", "agent.error"])
        if not url or event_name not in events:
            continue
        for attempt in range(3):
            req = urlrequest.Request(
                url,
                data=body,
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            try:
                def _post() -> None:
                    # SECURITY: Use standardized timeout (S4-005 fix)
                    with urlrequest.urlopen(req, timeout=TIMEOUT_CONFIG["webhook"]) as resp:
                        resp.read()
                await asyncio.to_thread(_post)
                break
            except urlerror.URLError:
                await asyncio.sleep(0.5 * (attempt + 1))
                continue


async def post_json(url: str, payload: Dict[str, Any]) -> None:
    body = json.dumps(payload).encode("utf-8")
    req = urlrequest.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        def _post() -> None:
            # SECURITY: Use standardized timeout (S4-005 fix)
            with urlrequest.urlopen(req, timeout=TIMEOUT_CONFIG["api_call"]) as resp:
                resp.read()
        await asyncio.to_thread(_post)
    except urlerror.URLError:
        logger.debug("post_json failed for URL")
        return


async def push_voice_tts(session_id: str, content: str) -> None:
    meta = load_session_meta(session_id)
    if meta.get("channel_id") != "voice":
        return
    payload = {"event": "tts", "session_id": session_id, "content": content}
    tts_cfg = STATE.config.get("gateway", {}).get("voice", {}).get("tts", {})
    if tts_cfg.get("mode") == "command" and tts_cfg.get("command"):
        audio_path = await run_voice_command(
            tts_cfg.get("command", ""),
            {"text": content, "session_id": session_id},
        )
        if audio_path:
            payload["audio_path"] = audio_path
    connections = STATE.voice_connections.get(session_id, [])
    for ws in list(connections):
        try:
            await ws.send_text(json.dumps(payload))
        except Exception:
            continue


def _sanitize_voice_vars(vars_map: Dict[str, str]) -> Dict[str, str]:
    """SECURITY: Sanitize voice command variables to prevent shell injection."""
    sanitized = {}
    for key, value in vars_map.items():
        # Use shlex.quote to escape shell metacharacters
        sanitized[key] = shlex.quote(str(value))
    return sanitized


async def run_voice_command(command: str, vars_map: Dict[str, str]) -> str:
    if not command:
        return ""
    # SECURITY: Validate command allowlist BEFORE variable substitution
    allowlist = STATE.config.get("gateway", {}).get("voice", {}).get("command_allowlist", [])
    if not allowlist:
        # SECURITY: Require explicit allowlist for voice commands
        return ""
    # Extract the base command from the template (first word before any {var})
    base_cmd = command.split()[0].split("{")[0]
    if Path(base_cmd).name not in allowlist and base_cmd not in allowlist:
        return ""
    # SECURITY: Sanitize all input variables to prevent injection
    sanitized_vars = _sanitize_voice_vars(vars_map)
    try:
        rendered = command.format(**sanitized_vars)
    except (KeyError, ValueError):
        return ""
    args = shlex.split(rendered)
    if not args:
        return ""
    # Double-check the executable after rendering
    if Path(args[0]).name not in allowlist:
        return ""
    try:
        proc = await asyncio.create_subprocess_exec(
            *args, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
        )
    except Exception:
        return ""
    stdout, _stderr = await proc.communicate()
    if proc.returncode != 0:
        return ""
    output = stdout.decode("utf-8", errors="ignore").strip()
    return output


def split_message(content: str, max_len: int) -> List[str]:
    if max_len <= 0:
        return [content]
    if len(content) <= max_len:
        return [content]
    chunks: List[str] = []
    start = 0
    while start < len(content):
        end = min(start + max_len, len(content))
        chunks.append(content[start:end])
        start = end
    return chunks


async def deliver_to_channel(session_id: str, content: str) -> None:
    meta = load_session_meta(session_id)
    channel_id = meta.get("channel_id", "webchat")
    if channel_id == "webchat":
        return
    adapter = STATE.adapters.get(channel_id)
    if not adapter:
        return
    channel_cfg = STATE.config.get("gateway", {}).get("channels", {}).get(channel_id, {})
    max_len = int(channel_cfg.get("max_length", 3500))
    for chunk in split_message(content, max_len):
        try:
            adapter.send(chunk, session_id)
        except Exception:
            continue


def validate_canvas(state: Dict[str, Any]) -> List[str]:
    errors: List[str] = []
    if "widgets" in state and not isinstance(state.get("widgets"), list):
        errors.append("widgets must be a list")
    for idx, widget in enumerate(state.get("widgets", [])):
        if not isinstance(widget, dict):
            errors.append(f"widget[{idx}] must be an object")
            continue
        if "type" not in widget:
            errors.append(f"widget[{idx}] missing type")
    return errors


def is_exec_allowed(meta: Dict[str, Any], config: Dict[str, Any]) -> bool:
    exec_cfg = config["gateway"].get("exec", {})
    policy = exec_cfg.get("policy", "allowlist")

    if policy == "deny":
        return False
    if policy == "full":
        return True

    agent_id = meta.get("agent", "")
    identity = STATE.identity_manager.get_identity(agent_id)

    # HITL Gatekeeper: Elevated agents bypass checks, restricted always ask
    if identity.trust_level == "elevated":
        return True

    approved = bool(meta.get("approved"))
    if identity.trust_level == "restricted" and not approved:
        return False

    ask = exec_cfg.get("ask", "on-miss")
    ask_fallback = exec_cfg.get("ask_fallback", "deny")
    elevated_mode = exec_cfg.get("elevated", "ask")
    allowlist = set(exec_cfg.get("allowlist", []))
    elevated = meta.get("elevated") == "full"

    if elevated:
        if elevated_mode == "full":
            return True
        if elevated_mode == "off":
            return False

    # Trusted agents follow allowlist or fallback to legacy policy
    if identity.trust_level == "trusted" or agent_id in allowlist:  # pragma: no cover
        return True

    if ask == "off":
        return ask_fallback == "full"
    if ask == "always" and not approved:
        return False
    if ask == "on-miss" and not approved:
        return False
    return approved or ask_fallback == "full"


def is_message_allowed(meta: Dict[str, Any], config: Dict[str, Any]) -> bool:
    scope = meta.get("scope", "dm")
    channel_id = meta.get("channel_id", "")
    policy = config["gateway"].get("policy", {})
    mention_required = policy.get("mention_required", True)
    allowlist = set(policy.get("allowlist", []))
    mentions = meta.get("mentions_bot", False)
    sender = meta.get("sender", "")

    if scope == "dm":
        return True
    if allowlist and sender not in allowlist:  # pragma: no cover
        return False
    if mention_required and not mentions:  # pragma: no cover
        return False
    return True


async def process_inbound(session_id: str, content: str, meta: Dict[str, Any], config: Dict[str, Any]) -> None:
    ensure_session(session_id)
    entry = append_message(session_id, "user", content)
    session_meta = load_session_meta(session_id)
    session_meta.update(
        {
            "session_id": session_id,
            "channel_id": meta.get("channel_id", "unknown"),
            "scope": meta.get("scope", "dm"),
            "owner": meta.get("agent", "architect"),
            "updated_at": timestamp(),
        }
    )
    persist_session_meta(session_id, session_meta)
    if not is_message_allowed(meta, config):
        audit_log({"ts": timestamp(), "event": "policy_block", "session_id": session_id})
        return
    run_id = f"run_{int(time.time() * 1000)}"
    if not is_exec_allowed(meta, config):
        pending = {
            "run_id": run_id,
            "session_id": session_id,
            "agent_id": meta.get("agent", "architect"),
            "requested_at": timestamp(),
            "meta": meta,
            "content": content,
            "approved": False,
        }
        STATE.pending_approvals[run_id] = pending
        persist_approval(run_id, pending)
        audit_log({"ts": timestamp(), "event": "approval_requested", "run_id": run_id})
        return
    if not STATE.connections:
        return
    conn_id, ws = next(iter(STATE.connections.items()))
    seq_state = {"value": STATE.conn_seq.get(conn_id, 0)}
    task = asyncio.create_task(dispatch_with_lock(ws, seq_state, session_id, run_id, meta, content, config))
    STATE.running[run_id] = task
    STATE.conn_seq[conn_id] = seq_state["value"]


async def send_result(ws: WebSocket, req_id: str, result: Dict[str, Any]) -> None:
    await ws.send_text(
        json.dumps(
            {
                "type": "response",
                "id": req_id,
                "ok": True,
                "result": result,
            }
        )
    )


async def send_error(ws: WebSocket, req_id: str, code: str, message: str) -> None:
    await ws.send_text(
        json.dumps(
            {
                "type": "response",
                "id": req_id,
                "ok": False,
                "error": {"code": code, "message": message},
            }
        )
    )


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
        # SECURITY: Use standardized timeout (S4-005 fix)
        resp = httpx.get(url, timeout=TIMEOUT_CONFIG["health_check"])
        status = {"health": resp.text, "channels": list(config["gateway"].get("channels", {}).keys())}
        print(json.dumps(status, indent=2))
        return 0 if resp.status_code == 200 else 1
    except Exception:
        print("Gateway not responding")
        return 1


def cmd_sessions(args: argparse.Namespace) -> int:
    config = resolve_config(args)
    bind = config["gateway"]["bind"]
    port = config["gateway"]["port"]
    url = f"http://{bind}:{port}/sessions"
    try:
        import httpx
        # SECURITY: Use standardized timeout (S4-005 fix)
        resp = httpx.get(url, timeout=TIMEOUT_CONFIG["health_check"])
        print(resp.text)
        return 0 if resp.status_code == 200 else 1
    except Exception:
        print("Gateway not responding")
        return 1


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
