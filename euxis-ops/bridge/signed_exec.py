#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Verify detached signature then execute command under bridge policy."""

from __future__ import annotations

import argparse
import base64
import binascii
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from bridge_common import append_jsonl, expand_user_path, utc_ts


def _decode_signature(raw: bytes, encoding: str) -> bytes:
    if encoding == "raw":
        return raw
    if encoding == "base64":
        return base64.b64decode(raw)
    if encoding == "hex":
        return binascii.unhexlify(raw.strip())
    raise ValueError(f"unsupported signature encoding: {encoding}")


def verify_signature(script_path: Path, sig_path: Path, pubkey_path: Path, sig_encoding: str) -> bool:
    message = script_path.read_bytes()
    signature = _decode_signature(sig_path.read_bytes(), sig_encoding)

    # Preferred path: Euxis crypto Rust core.
    try:
        import crypto_lib_rs  # type: ignore

        public_key = pubkey_path.read_bytes()
        return bool(crypto_lib_rs.ed25519_verify(public_key, message, signature))
    except ModuleNotFoundError:
        pass

    # Fallback for environments where Rust module is unavailable.
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

    pub = serialization.load_pem_public_key(pubkey_path.read_bytes())
    if not isinstance(pub, Ed25519PublicKey):
        return False

    try:
        pub.verify(signature, message)
        return True
    except Exception:
        return False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--script", required=True)
    parser.add_argument("--sig", required=True)
    parser.add_argument("--public-key", required=True)
    parser.add_argument("--sig-encoding", choices=["base64", "hex", "raw"], default="base64")
    parser.add_argument("--timeout-seconds", type=int, default=120)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    script_path = Path(args.script).resolve()
    sig_path = Path(args.sig).resolve()
    pubkey_path = Path(args.public_key).resolve()

    if not script_path.exists() or not sig_path.exists() or not pubkey_path.exists():
        print("missing script/signature/public-key file", file=sys.stderr)
        return 2

    verified = verify_signature(script_path, sig_path, pubkey_path, args.sig_encoding)

    audit_path = expand_user_path("~/.euxis/euxis-data/security/bridge-audit.jsonl")
    append_jsonl(
        audit_path,
        {
            "ts": utc_ts(),
            "event": "signed_exec.verify",
            "script": str(script_path),
            "sig": str(sig_path),
            "public_key": str(pubkey_path),
            "verified": bool(verified),
        },
    )

    if not verified:
        print(json.dumps({"status": "blocked", "reason": "signature_verification_failed"}))
        return 10

    if not args.command:
        print(json.dumps({"status": "verified", "script": str(script_path)}))
        return 0

    if args.command[0] == "--":
        cmd = args.command[1:]
    else:
        cmd = args.command

    if not cmd:
        print(json.dumps({"status": "verified", "script": str(script_path)}))
        return 0

    proc = subprocess.run(cmd, timeout=args.timeout_seconds, check=False)
    append_jsonl(
        audit_path,
        {
            "ts": utc_ts(),
            "event": "signed_exec.run",
            "script": str(script_path),
            "command": cmd,
            "return_code": proc.returncode,
        },
    )
    return int(proc.returncode)


if __name__ == "__main__":
    raise SystemExit(main())
