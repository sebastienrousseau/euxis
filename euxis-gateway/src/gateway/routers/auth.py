
from fastapi import APIRouter

def setup_auth(router: APIRouter, deps: dict):
    globals().update(deps)

    @router.get("/webhooks")
    async def webhooks_list(request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        return JSONResponse({"webhooks": STATE.webhooks})

    @router.post("/webhooks")
    async def webhooks_set(payload: Dict[str, Any], request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        hooks = payload.get("webhooks", [])
        if not isinstance(hooks, list):
            return JSONResponse({"status": "invalid"}, status_code=400)
        STATE.webhooks = hooks
        persist_webhooks(hooks)
        return JSONResponse({"status": "ok", "webhooks": hooks})

    slack_events_path = slack_cfg.get("events_path", "/channels/slack/events")
    telegram_webhook_path = telegram_cfg.get("webhook_path", "/channels/telegram/webhook")

    @router.post(slack_events_path)
    async def slack_events(request: Request) -> JSONResponse:
        adapter = STATE.adapters.get("slack")
        if not adapter:
            return JSONResponse({"status": "disabled"}, status_code=404)
        body = await request.body()
        try:
            payload = json.loads(body.decode("utf-8") or "{}")
        except json.JSONDecodeError:
            return JSONResponse({"status": "invalid"}, status_code=400)

        if payload.get("type") == "url_verification":
            return JSONResponse({"challenge": payload.get("challenge", "")})

        signing_secret = slack_cfg.get("signing_secret", "")
        if signing_secret and not verify_slack_signature(request, body, signing_secret):
            return JSONResponse({"status": "unauthorized"}, status_code=401)

        adapter.receive(payload)
        return JSONResponse({"status": "ok"})

    @router.post(telegram_webhook_path)
    async def telegram_webhook(payload: Dict[str, Any], request: Request) -> JSONResponse:
        adapter = STATE.adapters.get("telegram")
        if not adapter:
            return JSONResponse({"status": "disabled"}, status_code=404)
        secret = telegram_cfg.get("secret_token", "")
        if secret:
            header = request.headers.get("x-telegram-bot-api-secret-token", "")
            if header != secret:
                return JSONResponse({"status": "unauthorized"}, status_code=401)
        adapter.receive(payload)
        return JSONResponse({"status": "ok"})


    @router.post("/admin/exec")
    async def update_exec_policy(payload: Dict[str, Any], request: Request) -> JSONResponse:
        # SECURITY: Admin endpoints MUST require authentication
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        # SECURITY: Require re-authentication for admin operations via special header
        admin_token = config["gateway"].get("admin", {}).get("token", "")
        if admin_token:
            provided = request.headers.get("x-admin-token", "")
            if not hmac.compare_digest(provided, admin_token):
                audit_log({"ts": timestamp(), "event": "admin_auth_failed", "endpoint": "/admin/exec"})
                return JSONResponse({"status": "unauthorized", "reason": "admin_token_required"}, status_code=401)
        exec_cfg = config["gateway"].setdefault("exec", {})
        for key in ("policy", "ask", "ask_fallback", "allowlist"):
            if key in payload:
                exec_cfg[key] = payload[key]
        audit_log({"ts": timestamp(), "event": "admin_exec_policy_updated", "exec": exec_cfg})
        return JSONResponse({"status": "ok", "exec": exec_cfg})

