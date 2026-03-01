#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Signature lifecycle tooling for bridge script execution policies."""

from __future__ import annotations

import argparse
import base64
import binascii
import json
import sys
from pathlib import Path


def _load_crypto() -> tuple[object | None, object | None, object | None]:
    try:
        import crypto_lib_rs  # type: ignore

        return crypto_lib_rs, None, None
    except ModuleNotFoundError:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

        return None, serialization, Ed25519PrivateKey


def generate_keypair(private_path: Path, public_path: Path) -> None:
    crypto_lib_rs, serialization, Ed25519PrivateKey = _load_crypto()

    if crypto_lib_rs is not None:
        secret, public = crypto_lib_rs.generate_ed25519_keypair()
        private_path.write_bytes(base64.b64encode(secret))
        public_path.write_bytes(base64.b64encode(public))
        return

    private_key = Ed25519PrivateKey.generate()
    private_bytes = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    public_bytes = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    private_path.write_bytes(private_bytes)
    public_path.write_bytes(public_bytes)


def _decode_signature(raw: bytes, encoding: str) -> bytes:
    if encoding == "raw":
        return raw
    if encoding == "base64":
        return base64.b64decode(raw)
    if encoding == "hex":
        return binascii.unhexlify(raw.strip())
    raise ValueError(f"unsupported signature encoding: {encoding}")


def _encode_signature(raw: bytes, encoding: str) -> bytes:
    if encoding == "raw":
        return raw
    if encoding == "base64":
        return base64.b64encode(raw)
    if encoding == "hex":
        return binascii.hexlify(raw)
    raise ValueError(f"unsupported signature encoding: {encoding}")


def sign_detached(script_path: Path, private_path: Path, sig_path: Path, encoding: str) -> None:
    message = script_path.read_bytes()
    crypto_lib_rs, serialization, _ = _load_crypto()

    if crypto_lib_rs is not None:
        secret = base64.b64decode(private_path.read_bytes())
        signature = crypto_lib_rs.ed25519_sign(secret, message)
        sig_path.write_bytes(_encode_signature(signature, encoding))
        return

    from cryptography.hazmat.primitives import serialization as _serialization

    private_key = _serialization.load_pem_private_key(private_path.read_bytes(), password=None)
    signature = private_key.sign(message)
    sig_path.write_bytes(_encode_signature(signature, encoding))


def verify_detached(script_path: Path, sig_path: Path, public_path: Path, encoding: str) -> bool:
    message = script_path.read_bytes()
    signature = _decode_signature(sig_path.read_bytes(), encoding)
    crypto_lib_rs, serialization, _ = _load_crypto()

    if crypto_lib_rs is not None:
        public_key = base64.b64decode(public_path.read_bytes())
        return bool(crypto_lib_rs.ed25519_verify(public_key, message, signature))

    from cryptography.hazmat.primitives import serialization as _serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

    pub = _serialization.load_pem_public_key(public_path.read_bytes())
    if not isinstance(pub, Ed25519PublicKey):
        return False
    try:
        pub.verify(signature, message)
        return True
    except Exception:
        return False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    k = sub.add_parser("keygen")
    k.add_argument("--private-key", required=True)
    k.add_argument("--public-key", required=True)

    s = sub.add_parser("sign-script")
    s.add_argument("--script", required=True)
    s.add_argument("--private-key", required=True)
    s.add_argument("--sig", required=True)
    s.add_argument("--sig-encoding", choices=["base64", "hex", "raw"], default="base64")

    v = sub.add_parser("verify-script")
    v.add_argument("--script", required=True)
    v.add_argument("--public-key", required=True)
    v.add_argument("--sig", required=True)
    v.add_argument("--sig-encoding", choices=["base64", "hex", "raw"], default="base64")

    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.command == "keygen":
        prv = Path(args.private_key).resolve()
        pub = Path(args.public_key).resolve()
        prv.parent.mkdir(parents=True, exist_ok=True)
        pub.parent.mkdir(parents=True, exist_ok=True)
        generate_keypair(prv, pub)
        print(json.dumps({"status": "ok", "private_key": str(prv), "public_key": str(pub)}))
        return 0

    if args.command == "sign-script":
        script = Path(args.script).resolve()
        prv = Path(args.private_key).resolve()
        sig = Path(args.sig).resolve()
        sig.parent.mkdir(parents=True, exist_ok=True)
        sign_detached(script, prv, sig, args.sig_encoding)
        print(json.dumps({"status": "ok", "script": str(script), "sig": str(sig)}))
        return 0

    if args.command == "verify-script":
        ok = verify_detached(
            Path(args.script).resolve(),
            Path(args.sig).resolve(),
            Path(args.public_key).resolve(),
            args.sig_encoding,
        )
        print(json.dumps({"status": "ok" if ok else "blocked", "verified": bool(ok)}))
        return 0 if ok else 10

    return 2


if __name__ == "__main__":
    raise SystemExit(main())
