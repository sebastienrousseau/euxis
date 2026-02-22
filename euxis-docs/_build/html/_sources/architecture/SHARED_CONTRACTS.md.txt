# Shared Contracts (v0.0.1)

This document defines the interface contracts between Euxis modules. These contracts must be honored during and after the multi-repo extraction to ensure compatibility.

## Contract Overview

| Contract | Producer | Consumer | Type |
|----------|----------|----------|------|
| Adapter Registry | adapters | api/gateway | Python API |
| Crypto Bridge | crypto | packages/shared | Python API |
| Gateway Utils | packages/shared | api, adapters | Python API |
| Metrics Schema | metrics | all modules | JSON Schema |
| Agent Registry | core | cli, ui | JSON/SQLite |

---

## 1. Adapter Registry Contract

**Producer:** `adapters/src/adapters/registry.py`
**Consumer:** `packages/shared/src/shared/adapter_loader.py`

### Interface

```python
# adapters.registry.build_adapters() -> dict[str, Any]
def build_adapters() -> dict[str, Any]:
    """
    Returns a dictionary of adapter instances keyed by adapter name.

    Returns:
        dict with keys like "slack", "telegram" mapping to adapter instances
    """
```

### Contract Tests

```python
# tests/contracts/test_adapter_registry.py
def test_build_adapters_returns_dict():
    from adapters.registry import build_adapters
    result = build_adapters()
    assert isinstance(result, dict)

def test_adapter_instances_have_required_methods():
    from adapters.registry import build_adapters
    for name, adapter in build_adapters().items():
        assert hasattr(adapter, 'send_message')
        assert hasattr(adapter, 'receive_message')
```

---

## 2. Crypto Bridge Contract

**Producer:** `crypto/src/crypto_lib/`
**Consumer:** `packages/shared/src/shared/crypto_bridge.py`

### Interface

```python
# crypto_lib.encrypt(data, key, algorithm) -> bytes | str
# crypto_lib.decrypt(payload, key) -> bytes | str

def encrypt(data: str | bytes, key: bytes, algorithm: str = "AES-256-GCM") -> Any:
    """Encrypt data with the given key and algorithm."""

def decrypt(payload: str | bytes, key: bytes, algorithm: str = "AES-256-GCM") -> Any:
    """Decrypt payload with the given key."""

# crypto_lib.exceptions module must export:
class CryptoError(Exception): ...
class DecryptionError(CryptoError): ...
class InvalidKeyError(CryptoError): ...
```

### Contract Tests

```python
# tests/contracts/test_crypto_bridge.py
def test_encrypt_decrypt_roundtrip():
    from crypto_lib import encrypt, decrypt
    key = b'0' * 32  # 256-bit key
    plaintext = "test message"
    ciphertext = encrypt(plaintext, key)
    decrypted = decrypt(ciphertext, key)
    assert decrypted == plaintext.encode() or decrypted == plaintext

def test_exceptions_importable():
    from crypto_lib.exceptions import CryptoError, DecryptionError, InvalidKeyError
    assert issubclass(DecryptionError, CryptoError)
    assert issubclass(InvalidKeyError, CryptoError)
```

---

## 3. Gateway Utils Contract

**Producer:** `packages/shared/src/shared/gateway_utils.py`
**Consumer:** `api/src/gateway/`, `adapters/`

### Interface

```python
# Session Management
def make_session_id(channel_id: str, chat_id: str, thread_id: str | None = None,
                   dm_scope: str = "main", account_id: str | None = None,
                   sender_id: str | None = None) -> str: ...

def load_session_from_disk(session_id: str) -> list[dict[str, Any]]: ...
def persist_message(session_id: str, entry: dict[str, Any]) -> None: ...

# Directory Helpers
def gateway_data_dir() -> Path: ...
def sessions_dir() -> Path: ...
def runs_dir() -> Path: ...
def approvals_dir() -> Path: ...

# Utilities
def timestamp() -> str: ...  # Returns ISO8601 timestamp
```

### Contract Tests

```python
# tests/contracts/test_gateway_utils.py
def test_make_session_id_format():
    from shared.gateway_utils import make_session_id
    sid = make_session_id("slack", "C123")
    assert ":" in sid
    assert sid == "slack:C123"

def test_timestamp_iso8601():
    from shared.gateway_utils import timestamp
    ts = timestamp()
    # Should be parseable as ISO8601
    from datetime import datetime
    datetime.fromisoformat(ts.replace('Z', '+00:00'))

def test_directory_helpers_return_paths():
    from shared.gateway_utils import gateway_data_dir, sessions_dir
    from pathlib import Path
    assert isinstance(gateway_data_dir(), Path)
    assert isinstance(sessions_dir(), Path)
```

---

## 4. Metrics Schema Contract

**Producer:** `metrics/src/metrics/schemas/agent-performance.json`
**Consumer:** All modules emitting metrics

### Schema Version

Current schema version: `1.0.0`

### Required Event Types

All modules emitting performance events must conform to these schemas:

```json
{
  "Agent:TaskStarted": {
    "required": ["correlation_id", "timestamp", "agent_id", "session_id", "task_type", "priority"]
  },
  "Agent:TaskCompleted": {
    "required": ["correlation_id", "timestamp", "agent_id", "session_id", "duration_ms", "status"]
  },
  "Agent:TaskFailed": {
    "required": ["correlation_id", "timestamp", "agent_id", "session_id", "duration_ms", "failure_reason", "error_category"]
  }
}
```

### Contract Tests

```python
# tests/contracts/test_metrics_schema.py
import json
from jsonschema import validate

def test_schema_version_present():
    with open("metrics/src/metrics/schemas/agent-performance.json") as f:
        schema = json.load(f)
    assert "schema_version" in schema
    assert schema["schema_version"] == "1.0.0"

def test_required_events_defined():
    with open("metrics/src/metrics/schemas/agent-performance.json") as f:
        schema = json.load(f)
    events = schema["metrics_taxonomy"]["performance_events"]
    assert "Agent:TaskStarted" in events
    assert "Agent:TaskCompleted" in events
    assert "Agent:TaskFailed" in events
```

---

## 5. CLI ↔ Core Contract

**Producer:** `core/lib/*.sh` (shell libraries)
**Consumer:** `cli/bin/euxis*` (CLI tools)

### Environment Contract

```bash
# Required environment variables
EUXIS_HOME="${EUXIS_HOME:-$HOME/.euxis}"

# Required sourcing pattern
source "$EUXIS_HOME/core/lib/common.sh"
source "$EUXIS_HOME/core/lib/validation.sh"
```

### Function Contracts

```bash
# core/lib/common.sh
log_info() # (message: string) -> void
log_error() # (message: string) -> void
log_debug() # (message: string) -> void

# core/lib/validation.sh
validate_comprehensive() # () -> exit_code
validation_pass() # (message: string) -> void
validation_error() # (message: string) -> void
```

### Contract Tests

```bash
# tests/contracts/test_cli_core.bats
@test "common.sh exports log functions" {
    source "$EUXIS_HOME/core/lib/common.sh"
    type log_info
    type log_error
    type log_debug
}

@test "validation.sh exports validate functions" {
    source "$EUXIS_HOME/core/lib/validation.sh"
    type validate_comprehensive
    type validation_pass
    type validation_error
}
```

---

## 6. TUI ↔ Core Contract

**Producer:** `core/` (agent registry, session management)
**Consumer:** `ui/src/tui/` (TUI application)

### Agent Registry Contract

The TUI reads agent configuration from:
- `$EUXIS_HOME/agents/registry.json` (JSON format)
- `$EUXIS_HOME/agents/registry.db` (SQLite format)

```python
# Expected registry.json structure
{
    "schema_version": str,
    "protocol_version": str,
    "agents": [
        {
            "id": str,           # Required
            "name": str,         # Required
            "description": str,  # Required
            "status": str,       # "active" | "inactive" | "pending"
            "capabilities": list[str]
        }
    ]
}
```

### Contract Tests

```python
# tests/contracts/test_tui_core.py
def test_registry_json_structure():
    import json
    with open("agents/registry.json") as f:
        registry = json.load(f)
    assert "schema_version" in registry
    assert "protocol_version" in registry
    assert "agents" in registry
    for agent in registry["agents"]:
        assert "id" in agent
        assert "name" in agent
```

---

## Contract Enforcement

### CI Integration

Add to `.github/workflows/contract-tests.yml`:

```yaml
name: Contract Tests
on: [push, pull_request]
jobs:
  contracts:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - run: pip install pytest jsonschema
      - run: pytest tests/contracts/ -v
```

### Breaking Change Detection

Before extraction, run contract tests against both:
1. Current monorepo (baseline)
2. Each extracted module (regression)

Any contract test failure blocks extraction.

---

## Version Compatibility Matrix

| euxis-core | euxis-cli | euxis-ui | euxis-api | euxis-adapters |
|------------|-----------|----------|-----------|----------------|
| 0.1.0      | 0.1.0     | 0.1.0    | 0.1.0     | 0.1.0          |

All modules must maintain compatible versions during initial extraction. After extraction, semantic versioning will govern compatibility.

---

*Document created: 2026-02-16 as part of v0.0.1 multi-repo preparation*
