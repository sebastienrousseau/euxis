# Euxis Gateway Security Audit Report

**Document ID:** EUXIS-SEC-current-02
**Classification:** Internal - Security
**Audit Date:** February **Auditor:** Security Engineering Team
**Status:** COMPLETED - All S1 Findings Remediated

---

## Executive Summary

This security audit report documents the identification, analysis, and remediation of four Severity 1 (S1) critical vulnerabilities discovered in the Euxis Gateway server component (`euxis-gateway/src/gateway/server.py`). All vulnerabilities have been successfully remediated and verified.

### Key Metrics

| Metric | Value |
|--------|-------|
| Total Findings | 4 |
| Severity 1 (Critical) | 4 |
| Severity 2 (High) | 0 |
| Remediated | 4 (100%) |
| Verified | 4 (100%) |

### Risk Summary

Prior to remediation, the gateway was vulnerable to:
- **Authentication bypass** via default empty tokens
- **Privilege escalation** via unauthenticated admin endpoints
- **Remote code execution** via voice command injection
- **Arbitrary file upload** enabling potential server compromise

All identified vulnerabilities have been addressed with defense-in-depth controls.

---

## Findings Summary

| ID | Title | Severity | CVSS | Status | Location |
|----|-------|----------|------|--------|----------|
| EUXIS-current-001 | Default Empty Authentication Tokens | S1/Critical | 9.8 | FIXED | `is_authorized()`, `is_http_authorized()` |
| EUXIS-current-002 | Admin Policy Override Without Authentication | S1/Critical | 9.1 | FIXED | `/admin/exec` endpoint |
| EUXIS-current-003 | Voice Command Injection | S1/Critical | 9.8 | FIXED | `run_voice_command()` |
| EUXIS-current-004 | Unrestricted File Upload | S1/Critical | 8.8 | FIXED | `/voice/upload` endpoint |

---

## Detailed Findings

### EUXIS-current-001: Default Empty Authentication Tokens

**Severity:** S1 - Critical
**CVSS v3.1 Score:** 9.8 (Critical)
**CVSS Vector:** `AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H`
**CWE:** CWE-287 (Improper Authentication)
**Location:** `${EUXIS_HOME}/euxis-gateway/src/gateway/server.py` lines 926-1002

#### Description

The `is_authorized()` and `is_http_authorized()` functions previously returned `True` when no authentication token or password was configured. This allowed any unauthenticated request to access protected endpoints when the gateway was deployed with default configuration.

#### Impact

- Complete bypass of authentication controls
- Unauthorized access to all gateway endpoints
- Session hijacking and data exfiltration
- Remote command execution via agent dispatch

#### Technical Details

**Vulnerable Code Pattern (BEFORE):**

```python
def is_authorized(ws: WebSocket, config: Dict[str, Any]) -> Tuple[bool, Optional[str]]:
    mode = config["gateway"]["auth"].get("mode", "token")
    if mode == "token":
        token = config["gateway"]["auth"].get("token", {}).get("value", "")
        if not token:
            # VULNERABLE: Empty token was implicitly allowed
            return True, None  # <-- BUG: Should deny, not allow
        # ... rest of validation
```

**Fixed Code (AFTER):**

```python
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
        # ... rest of validation
    if mode == "password":
        password = config["gateway"]["auth"].get("password", {}).get("value", "")
        if not password:
            # SECURITY: Empty password = DENY access (require explicit password configuration)
            return False, "auth_not_configured"
        # ... rest of validation
    if mode == "none":
        # Explicit opt-in to no authentication (must be configured deliberately)
        return True, None
    return False, "auth_not_configured"
```

#### Remediation

1. Empty token/password now returns `(False, "auth_not_configured")` instead of `(True, None)`
2. Added explicit `mode: "none"` for intentional authentication opt-out
3. Default behavior is now secure (deny-by-default)
4. Applied fix to both WebSocket (`is_authorized`) and HTTP (`is_http_authorized`) functions

#### Verification

```python
# Test: Empty token configuration
config = {"gateway": {"auth": {"mode": "token", "token": {"value": ""}}}}
result, reason = is_authorized(mock_ws, config)
assert result == False
assert reason == "auth_not_configured"

# Test: Explicit none mode (allowed)
config = {"gateway": {"auth": {"mode": "none"}}}
result, reason = is_authorized(mock_ws, config)
assert result == True
```

---

### EUXIS-current-002: Admin Policy Override Without Authentication

**Severity:** S1 - Critical
**CVSS v3.1 Score:** 9.1 (Critical)
**CVSS Vector:** `AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H`
**CWE:** CWE-306 (Missing Authentication for Critical Function)
**Location:** `${EUXIS_HOME}/euxis-gateway/src/gateway/server.py` line 714

#### Description

The `/admin/exec` endpoint allowed modification of execution policies without requiring authentication. An attacker could remotely modify the execution policy to `"full"` mode, bypassing all approval workflows and allowlist restrictions.

#### Impact

- Unrestricted agent execution
- Bypass of approval workflows
- Policy tampering
- Privilege escalation to full execution mode

#### Technical Details

**Vulnerable Code Pattern (BEFORE):**

```python
@app.post("/admin/exec")
async def update_exec_policy(payload: Dict[str, Any], request: Request) -> JSONResponse:
    # VULNERABLE: No authentication check!
    exec_cfg = config["gateway"].setdefault("exec", {})
    for key in ("policy", "ask", "ask_fallback", "allowlist"):
        if key in payload:
            exec_cfg[key] = payload[key]
    return JSONResponse({"status": "ok", "exec": exec_cfg})
```

**Fixed Code (AFTER):**

```python
@app.post("/admin/exec")
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
```

#### Remediation

1. Added `is_http_authorized()` check as primary authentication
2. Added optional `x-admin-token` header for elevated admin operations
3. Implemented audit logging for admin actions (both success and failure)
4. Uses `hmac.compare_digest()` for timing-safe token comparison

#### Verification

```bash
# Test: Unauthenticated request (should fail)
curl -X POST http://localhost:18789/admin/exec \
  -H "Content-Type: application/json" \
  -d '{"policy": "full"}'
# Expected: 401 Unauthorized

# Test: Authenticated request with admin token
curl -X POST http://localhost:18789/admin/exec \
  -H "Authorization: Bearer <token>" \
  -H "x-admin-token: <admin-token>" \
  -H "Content-Type: application/json" \
  -d '{"policy": "allowlist"}'
# Expected: 200 OK
```

---

### EUXIS-current-003: Voice Command Injection

**Severity:** S1 - Critical
**CVSS v3.1 Score:** 9.8 (Critical)
**CVSS Vector:** `AV:N/AC:L/PR:N/UI:N/S:C/C:H/I:H/A:H`
**CWE:** CWE-78 (Improper Neutralization of Special Elements used in an OS Command)
**Location:** `${EUXIS_HOME}/euxis-gateway/src/gateway/server.py` line 1417

#### Description

The `run_voice_command()` function accepted user-controlled input (file paths, session IDs) and substituted them into shell commands without sanitization. An attacker could inject shell metacharacters to execute arbitrary commands.

#### Impact

- Remote code execution (RCE)
- Server compromise
- Data exfiltration
- Lateral movement within infrastructure

#### Attack Vector Example

```python
# Malicious audio_path input
audio_path = "${TMPDIR:-/tmp}/audio.wav; rm -rf / #"

# Vulnerable command template
command = "whisper {audio_path}"

# Results in shell execution of:
# whisper ${TMPDIR:-/tmp}/audio.wav; rm -rf / #
```

#### Technical Details

**Vulnerable Code Pattern (BEFORE):**

```python
async def run_voice_command(command: str, vars_map: Dict[str, str]) -> str:
    if not command:
        return ""
    # VULNERABLE: No validation of command against allowlist
    # VULNERABLE: No sanitization of vars_map values
    try:
        rendered = command.format(**vars_map)  # Direct injection point!
    except (KeyError, ValueError):
        return ""
    args = shlex.split(rendered)
    # ... subprocess execution
```

**Fixed Code (AFTER):**

```python
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
    # ... subprocess execution
```

#### Remediation

1. Added `_sanitize_voice_vars()` function using `shlex.quote()` to escape all shell metacharacters
2. Made command allowlist mandatory (empty allowlist = deny all commands)
3. Validate command BEFORE variable substitution to prevent allowlist bypass
4. Double-check executable name AFTER rendering as defense-in-depth

#### Verification

```python
# Test: Injection attempt
vars_map = {"audio_path": "${TMPDIR:-/tmp}/test.wav; rm -rf /"}
sanitized = _sanitize_voice_vars(vars_map)
assert sanitized["audio_path"] == "'${TMPDIR:-/tmp}/test.wav; rm -rf /'"
# The entire malicious string is now quoted as a single argument

# Test: Empty allowlist blocks execution
allowlist = []
result = await run_voice_command("whisper {audio_path}", {"audio_path": "${TMPDIR:-/tmp}/test.wav"})
assert result == ""
```

---

### EUXIS-current-004: Unrestricted File Upload

**Severity:** S1 - Critical
**CVSS v3.1 Score:** 8.8 (High)
**CVSS Vector:** `AV:N/AC:L/PR:L/UI:N/S:U/C:H/I:H/A:H`
**CWE:** CWE-434 (Unrestricted Upload of File with Dangerous Type)
**Location:** `${EUXIS_HOME}/euxis-gateway/src/gateway/server.py` line 511

#### Description

The `/voice/upload` endpoint allowed uploading files with arbitrary extensions and content types. An attacker could upload malicious executables, scripts, or polyglot files disguised as audio.

#### Impact

- Malware upload
- Web shell deployment
- Server-side code execution
- Storage exhaustion (DoS)

#### Technical Details

**Vulnerable Code Pattern (BEFORE):**

```python
@app.post("/voice/upload")
async def voice_upload(session_id: str, file: UploadFile = File(...), request: Request = None):
    if not config["gateway"].get("voice", {}).get("enabled", True):
        return JSONResponse({"status": "disabled"}, status_code=404)
    # VULNERABLE: Authentication was optional!
    if request:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
    # VULNERABLE: No file type validation
    # VULNERABLE: No file size limit
    # VULNERABLE: No content validation
    blob = await file.read()  # Could read unlimited data
    path = persist_voice_blob(session_id, blob, file.filename.rsplit(".", 1)[-1])
    return JSONResponse({"status": "ok", "path": str(path)})
```

**Fixed Code (AFTER):**

```python
# SECURITY: Allowed audio file extensions for voice upload
VOICE_ALLOWED_EXTENSIONS = {"wav", "mp3", "ogg", "webm", "m4a", "flac", "aac"}
# SECURITY: Maximum file size for voice uploads (10MB)
VOICE_MAX_FILE_SIZE = 10 * 1024 * 1024

# SECURITY: Audio file magic byte signatures for content validation
AUDIO_MAGIC_BYTES = {
    "wav": [(0, b"RIFF"), (8, b"WAVE")],
    "mp3": [(0, b"\xff\xfb"), (0, b"\xff\xfa"), (0, b"\xff\xf3"), (0, b"\xff\xf2"), (0, b"ID3")],
    "ogg": [(0, b"OggS")],
    "flac": [(0, b"fLaC")],
    "m4a": [(4, b"ftyp")],
    "aac": [(0, b"\xff\xf1"), (0, b"\xff\xf9")],
    "webm": [(0, b"\x1a\x45\xdf\xa3")],
}

def _validate_audio_magic(blob: bytes, extension: str) -> bool:
    """SECURITY: Validate audio file content matches expected magic bytes."""
    if len(blob) < 12:
        return False
    signatures = AUDIO_MAGIC_BYTES.get(extension, [])
    if not signatures:
        return False
    for offset, magic in signatures:
        if len(blob) > offset + len(magic):
            if blob[offset : offset + len(magic)] == magic:
                return True
    return False

@app.post("/voice/upload")
async def voice_upload(session_id: str, file: UploadFile = File(...), request: Request = None):
    if not config["gateway"].get("voice", {}).get("enabled", True):
        return JSONResponse({"status": "disabled"}, status_code=404)
    # SECURITY: Authentication is ALWAYS required (removed optional check)
    if request is None:
        return JSONResponse({"status": "unauthorized", "reason": "missing_request"}, status_code=401)
    allowed, reason = is_http_authorized(request, config)
    if not allowed:
        return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
    if not session_id:
        return JSONResponse({"status": "invalid"}, status_code=400)
    # SECURITY: Validate file extension against allowlist
    filename = file.filename or "audio.wav"
    suffix = filename.rsplit(".", 1)[-1].lower() if "." in filename else ""
    if suffix not in VOICE_ALLOWED_EXTENSIONS:
        return JSONResponse(
            {"status": "invalid", "reason": f"file_type_not_allowed: {suffix}"},
            status_code=400,
        )
    # SECURITY: Read file with size limit to prevent DoS
    max_size = config["gateway"].get("voice", {}).get("max_upload_size", VOICE_MAX_FILE_SIZE)
    chunks = []
    total_size = 0
    async for chunk in file.stream():
        total_size += len(chunk)
        if total_size > max_size:
            return JSONResponse(
                {"status": "invalid", "reason": f"file_too_large: max {max_size} bytes"},
                status_code=413,
            )
        chunks.append(chunk)
    blob = b"".join(chunks)
    # SECURITY: Validate content magic bytes for audio files
    if not _validate_audio_magic(blob, suffix):
        return JSONResponse(
            {"status": "invalid", "reason": "content_type_mismatch"},
            status_code=400,
        )
    path = persist_voice_blob(session_id, blob, suffix)
    return JSONResponse({"status": "ok", "path": str(path)})
```

#### Remediation

1. **Extension allowlist:** Only allow wav, mp3, ogg, webm, m4a, flac, aac extensions
2. **File size limit:** 10MB maximum with streaming validation to prevent memory exhaustion
3. **Magic byte validation:** Verify file content matches expected audio format signatures
4. **Mandatory authentication:** Removed optional authentication check - auth is now always required
5. **Streaming validation:** Size check happens during streaming, not after full upload

#### Verification

```bash
# Test: Invalid extension (should fail)
curl -X POST "http://localhost:18789/voice/upload?session_id=test" \
  -H "Authorization: Bearer <token>" \
  -F "file=@malware.exe"
# Expected: 400 Bad Request - file_type_not_allowed

# Test: Size limit exceeded (should fail)
dd if=/dev/zero bs=1M count=15 of=${TMPDIR:-/tmp}/large.wav
curl -X POST "http://localhost:18789/voice/upload?session_id=test" \
  -H "Authorization: Bearer <token>" \
  -F "file=@${TMPDIR:-/tmp}/large.wav"
# Expected: 413 Payload Too Large

# Test: Content mismatch (should fail)
echo "not audio data" > ${TMPDIR:-/tmp}/fake.wav
curl -X POST "http://localhost:18789/voice/upload?session_id=test" \
  -H "Authorization: Bearer <token>" \
  -F "file=@${TMPDIR:-/tmp}/fake.wav"
# Expected: 400 Bad Request - content_type_mismatch

# Test: Valid audio file (should succeed)
curl -X POST "http://localhost:18789/voice/upload?session_id=test" \
  -H "Authorization: Bearer <token>" \
  -F "file=@/path/to/valid/audio.wav"
# Expected: 200 OK
```

---

## Compliance Implications

### SOC 2 Type II

| Control | Impact | Status |
|---------|--------|--------|
| CC6.1 - Logical Access Controls | Authentication bypass resolved | COMPLIANT |
| CC6.3 - Authorized Access | Admin endpoint secured | COMPLIANT |
| CC6.6 - System Operations | Input validation added | COMPLIANT |
| CC6.7 - Change Management | Audit logging implemented | COMPLIANT |
| CC7.2 - Security Monitoring | Failed auth logged | COMPLIANT |

### OWASP Top 10 (2021)

| Category | Finding | Status |
|----------|---------|--------|
| A01 - Broken Access Control | Empty token bypass | FIXED |
| A03 - Injection | Voice command injection | FIXED |
| A04 - Insecure Design | Missing auth on admin | FIXED |
| A05 - Security Misconfiguration | Default insecure config | FIXED |

### PCI DSS v4.0

| Requirement | Relevance | Status |
|-------------|-----------|--------|
| 6.3.1 - Input Validation | Command injection | COMPLIANT |
| 7.2.1 - Access Control | Auth requirements | COMPLIANT |
| 10.2.1 - Audit Logging | Admin action logging | COMPLIANT |

---

## Recommendations for Ongoing Security

### Immediate Actions

1. **Rotate all authentication tokens** - Assume tokens may have been compromised
2. **Review audit logs** - Check for suspicious admin policy changes
3. **Update gateway configuration** - Ensure explicit auth tokens are set

### Configuration Hardening

```json
{
  "gateway": {
    "auth": {
      "mode": "token",
      "token": {
        "value": "<generate-secure-random-token>",
        "allow_query_param": false
      }
    },
    "admin": {
      "token": "<separate-admin-token>"
    },
    "voice": {
      "enabled": true,
      "command_allowlist": ["whisper", "piper"],
      "max_upload_size": 10485760
    },
    "exec": {
      "policy": "allowlist",
      "ask": "always",
      "ask_fallback": "deny",
      "allowlist": ["architect", "reviewer"]
    }
  }
}
```

### Development Practices

1. **Security review for all auth-related changes**
2. **Input validation at all trust boundaries**
3. **Default-deny security posture**
4. **Comprehensive audit logging**
5. **Regular penetration testing**

### Monitoring Recommendations

1. Monitor for `auth_not_configured` errors in production
2. Alert on `admin_auth_failed` audit events
3. Track file upload rejection rates for anomaly detection
4. Review voice command execution patterns

---

## Timeline

| Date | Event |
|------|-------|
|-02-10 | Vulnerabilities identified during code review |
|-02-12 | S1 severity classification confirmed |
|-02-14 | Remediation development completed |
|-02-15 | Security testing and verification |
|-02-17 | Audit report finalized |

---

## Sign-Off

### Security Review

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Security Lead | __________________ | __________________ | __________ |
| Engineering Lead | __________________ | __________________ | __________ |
| QA Lead | __________________ | __________________ | __________ |

### Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| CISO | __________________ | __________________ | __________ |
| VP Engineering | __________________ | __________________ | __________ |

---

## Appendix A: File Locations

| Component | Path |
|-----------|------|
| Gateway Server | `${EUXIS_HOME}/euxis-gateway/src/gateway/server.py` |
| Gateway Config | `~/.euxis/euxis-policy/gateway.json` |
| Audit Logs | `~/.euxis/euxis-data/audit/` |

## Appendix B: Related Documentation

- [Gateway Configuration Guide](../reference/gateway-config.md)
- [SOC 2 Compliance Matrix](../compliance/SOC2-COMPLIANCE.md)
- [Security Architecture](../architecture.md)

---

*This document is confidential and intended for internal security review purposes only.*

**Document Version:** 1.0
**Last Updated:**-02-17
**Classification:** Internal - Security
