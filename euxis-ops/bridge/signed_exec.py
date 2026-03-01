#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Verify detached signature then execute command under bridge policy."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from bridge_common import append_jsonl, expand_user_path, utc_ts
from signature_tools import verify_detached


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

    verified = verify_detached(script_path, sig_path, pubkey_path, args.sig_encoding)

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

    cmd = args.command[1:] if args.command and args.command[0] == "--" else args.command
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
