from __future__ import annotations

import base64
import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
IMPORT_TOOL = REPO_ROOT / "euxis-ops" / "bridge" / "import_openclaw.py"
DAEMON_TOOL = REPO_ROOT / "euxis-ops" / "bridge" / "daemon.py"
SIGNED_EXEC_TOOL = REPO_ROOT / "euxis-ops" / "bridge" / "signed_exec.py"
SIGNATURE_TOOL = REPO_ROOT / "euxis-ops" / "bridge" / "signature_tools.py"


def _run(cmd: list[str], env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=False, text=True, capture_output=True, cwd=REPO_ROOT, env=env)


def _load_module(module_name: str, path: Path):
    spec = importlib.util.spec_from_file_location(module_name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_import_openclaw_dry_run(tmp_path: Path) -> None:
    source = tmp_path / "openclaw"
    (source / "credentials").mkdir(parents=True)
    (source / "sessions").mkdir(parents=True)
    (source / "notes").mkdir(parents=True)
    (source / "credentials" / "id.json").write_text('{"id":"a"}', encoding="utf-8")
    (source / "sessions" / "chat.jsonl").write_text('{"x":1}\n', encoding="utf-8")
    (source / "notes" / "memo.md").write_text('# memo\n', encoding="utf-8")

    out_root = tmp_path / "euxis"
    config = {
        "migration": {
            "output_roots": {
                "identities": str(out_root / "ids"),
                "sessions": str(out_root / "sessions"),
                "transcripts": str(out_root / "transcripts"),
            }
        }
    }
    cfg = tmp_path / "bridge.json"
    cfg.write_text(json.dumps(config), encoding="utf-8")

    manifest = tmp_path / "manifest.json"
    env = {**os.environ, "HOME": str(tmp_path)}
    proc = _run(
        [
            sys.executable,
            str(IMPORT_TOOL),
            "--config",
            str(cfg),
            "--source",
            str(source),
            "--dry-run",
            "--output-manifest",
            str(manifest),
        ],
        env=env,
    )
    assert proc.returncode == 0, proc.stderr
    payload = json.loads(proc.stdout)
    assert payload["files"] == 3

    data = json.loads(manifest.read_text(encoding="utf-8"))
    assert data["counts"]["total"] == 3
    assert not (out_root / "ids").exists()


def test_import_openclaw_copy(tmp_path: Path) -> None:
    source = tmp_path / "openclaw"
    (source / "credentials").mkdir(parents=True)
    (source / "credentials" / "id.json").write_text('{"id":"a"}', encoding="utf-8")

    out_root = tmp_path / "euxis"
    config = {
        "migration": {
            "output_roots": {
                "identities": str(out_root / "ids"),
                "sessions": str(out_root / "sessions"),
                "transcripts": str(out_root / "transcripts"),
            }
        }
    }
    cfg = tmp_path / "bridge.json"
    cfg.write_text(json.dumps(config), encoding="utf-8")

    manifest = tmp_path / "manifest.json"
    env = {**os.environ, "HOME": str(tmp_path)}
    proc = _run(
        [
            sys.executable,
            str(IMPORT_TOOL),
            "--config",
            str(cfg),
            "--source",
            str(source),
            "--output-manifest",
            str(manifest),
        ],
        env=env,
    )
    assert proc.returncode == 0, proc.stderr
    assert (out_root / "ids" / "credentials" / "id.json").exists()
    normalized = out_root / "ids" / "identities.normalized.jsonl"
    assert normalized.exists()
    assert "identity" in normalized.read_text(encoding="utf-8")


def test_import_openclaw_golden_layout_adapters(tmp_path: Path) -> None:
    source = tmp_path / "openclaw"
    (source / "credentials").mkdir(parents=True)
    (source / "agents" / "alpha").mkdir(parents=True)
    (source / "notes").mkdir(parents=True)
    (source / "credentials" / "profile.json").write_text(
        json.dumps({"agent_id": "alpha", "display_name": "Agent Alpha"}),
        encoding="utf-8",
    )
    (source / "agents" / "alpha" / "memory.json").write_text(
        json.dumps({"messages": [{"role": "user", "content": "hello"}]}),
        encoding="utf-8",
    )
    (source / "agents" / "alpha" / "session.jsonl").write_text(
        '{"role":"assistant","content":"hi"}\n',
        encoding="utf-8",
    )
    (source / "notes" / "memo.md").write_text("# hello\n", encoding="utf-8")

    out_root = tmp_path / "euxis"
    cfg = tmp_path / "bridge.json"
    cfg.write_text(
        json.dumps(
            {
                "migration": {
                    "output_roots": {
                        "identities": str(out_root / "ids"),
                        "sessions": str(out_root / "sessions"),
                        "transcripts": str(out_root / "transcripts"),
                    }
                }
            }
        ),
        encoding="utf-8",
    )
    env = {**os.environ, "HOME": str(tmp_path)}
    proc = _run(
        [sys.executable, str(IMPORT_TOOL), "--config", str(cfg), "--source", str(source)],
        env=env,
    )
    assert proc.returncode == 0, proc.stderr
    sessions_norm = out_root / "sessions" / "sessions.normalized.jsonl"
    ids_norm = out_root / "ids" / "identities.normalized.jsonl"
    assert sessions_norm.exists()
    assert ids_norm.exists()
    assert "Agent Alpha" in ids_norm.read_text(encoding="utf-8")
    assert "hello" in sessions_norm.read_text(encoding="utf-8")


def test_daemon_runtime_process(tmp_path: Path) -> None:
    euxis_data = tmp_path / "euxis-data"
    config = {
        "paths": {"euxis_data": str(euxis_data)},
        "bridge_daemon": {
            "inbound_channels": {"telegram": {"token_env": ""}},
            "gateway_forward": {"live_forward": False},
        },
        "security": {"audit": {"log_path": str(tmp_path / "audit.jsonl")}},
    }
    daemon_mod = _load_module("bridge_daemon", DAEMON_TOOL)
    runtime = daemon_mod.BridgeRuntime(config)
    result = runtime.process(
        "telegram",
        {
            "message": {
                "from": {"id": 123},
                "chat": {"id": 456},
                "text": "hello",
            }
        },
    )
    assert result["status"] == "forwarded"
    assert (euxis_data / "bridge" / "outbox.jsonl").exists()


def test_daemon_idempotency_and_dead_letter(tmp_path: Path) -> None:
    euxis_data = tmp_path / "euxis-data"
    config = {
        "paths": {"euxis_data": str(euxis_data)},
        "bridge_daemon": {"inbound_channels": {"telegram": {"token_env": ""}}},
        "security": {"audit": {"log_path": str(tmp_path / "audit.jsonl")}},
    }
    daemon_mod = _load_module("bridge_daemon_dlq", DAEMON_TOOL)
    runtime = daemon_mod.BridgeRuntime(config)
    runtime.forward_to_gateway = lambda _frame: (False, "simulated-failure")

    payload = {"message": {"from": {"id": 1}, "chat": {"id": 2}, "text": "x"}}
    first = runtime.process("telegram", payload, "idem-1")
    second = runtime.process("telegram", payload, "idem-1")

    assert first["status"] == "dead_letter"
    assert second["status"] == "duplicate"
    assert (euxis_data / "bridge" / "dead_letter.jsonl").exists()


def test_signed_exec_verifies_and_blocks(tmp_path: Path) -> None:
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

    script = tmp_path / "script.sh"
    script.write_text("#!/bin/sh\necho ok\n", encoding="utf-8")

    private_key = Ed25519PrivateKey.generate()
    public_key = private_key.public_key()

    pub_pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    pub_path = tmp_path / "public.pem"
    pub_path.write_bytes(pub_pem)

    sig = private_key.sign(script.read_bytes())
    sig_path = tmp_path / "script.sig"
    sig_path.write_bytes(base64.b64encode(sig))

    env = {**os.environ, "HOME": str(tmp_path)}
    proc_ok = _run(
        [
            sys.executable,
            str(SIGNED_EXEC_TOOL),
            "--script",
            str(script),
            "--sig",
            str(sig_path),
            "--public-key",
            str(pub_path),
            "--sig-encoding",
            "base64",
        ],
        env=env,
    )
    assert proc_ok.returncode == 0, proc_ok.stderr
    assert "verified" in proc_ok.stdout

    sig_path.write_bytes(base64.b64encode(b"bad"))
    proc_bad = _run(
        [
            sys.executable,
            str(SIGNED_EXEC_TOOL),
            "--script",
            str(script),
            "--sig",
            str(sig_path),
            "--public-key",
            str(pub_path),
            "--sig-encoding",
            "base64",
        ],
        env=env,
    )
    assert proc_bad.returncode == 10
    assert "blocked" in proc_bad.stdout


def test_signature_tools_lifecycle(tmp_path: Path) -> None:
    script = tmp_path / "script.sh"
    script.write_text("#!/bin/sh\necho ok\n", encoding="utf-8")
    private_key = tmp_path / "bridge-private.pem"
    public_key = tmp_path / "bridge-public.pem"
    sig_file = tmp_path / "script.sig"

    keygen = _run(
        [
            sys.executable,
            str(SIGNATURE_TOOL),
            "keygen",
            "--private-key",
            str(private_key),
            "--public-key",
            str(public_key),
        ]
    )
    assert keygen.returncode == 0, keygen.stderr
    assert private_key.exists()
    assert public_key.exists()

    sign = _run(
        [
            sys.executable,
            str(SIGNATURE_TOOL),
            "sign-script",
            "--script",
            str(script),
            "--private-key",
            str(private_key),
            "--sig",
            str(sig_file),
        ]
    )
    assert sign.returncode == 0, sign.stderr
    assert sig_file.exists()

    verify_ok = _run(
        [
            sys.executable,
            str(SIGNATURE_TOOL),
            "verify-script",
            "--script",
            str(script),
            "--public-key",
            str(public_key),
            "--sig",
            str(sig_file),
        ]
    )
    assert verify_ok.returncode == 0, verify_ok.stderr

    script.write_text("#!/bin/sh\necho changed\n", encoding="utf-8")
    verify_bad = _run(
        [
            sys.executable,
            str(SIGNATURE_TOOL),
            "verify-script",
            "--script",
            str(script),
            "--public-key",
            str(public_key),
            "--sig",
            str(sig_file),
        ]
    )
    assert verify_bad.returncode == 10
