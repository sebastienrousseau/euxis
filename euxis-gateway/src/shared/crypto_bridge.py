# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Dynamic crypto bridge to avoid hard module coupling."""
from __future__ import annotations

import importlib.util
from types import ModuleType
from typing import Any


class CryptoError(Exception):
    """Fallback crypto error when crypto_lib is unavailable."""


class DecryptionError(CryptoError):
    """Fallback decryption error."""


class InvalidKeyError(CryptoError):
    """Fallback invalid key error."""


def _load_crypto() -> tuple[ModuleType | None, ModuleType | None]:
    if importlib.util.find_spec("crypto_lib") is None:
        return None, None
    crypto_mod = importlib.import_module("crypto_lib")
    exc_mod = importlib.import_module("crypto_lib.exceptions")
    return crypto_mod, exc_mod


def is_available() -> bool:
    return importlib.util.find_spec("crypto_lib") is not None


def encrypt(data: str | bytes, key: bytes, algorithm: str = "AES-256-GCM") -> Any:
    crypto_mod, _exc_mod = _load_crypto()
    if crypto_mod is None:
        raise CryptoError("crypto_lib not available")
    return crypto_mod.encrypt(data, key, algorithm)


def decrypt(payload: str | bytes, key: bytes, algorithm: str = "AES-256-GCM") -> Any:
    crypto_mod, _exc_mod = _load_crypto()
    if crypto_mod is None:
        raise CryptoError("crypto_lib not available")
    return crypto_mod.decrypt(payload, key)


def crypto_errors() -> tuple[type[Exception], type[Exception], type[Exception]]:
    _crypto_mod, exc_mod = _load_crypto()
    if exc_mod is None:
        return CryptoError, DecryptionError, InvalidKeyError
    return exc_mod.CryptoError, exc_mod.DecryptionError, exc_mod.InvalidKeyError
