"""Tests for AAD (Additional Authenticated Data) encryption functions."""

import pytest

from crypto_lib.core import decrypt_aad, encrypt_aad
from crypto_lib.exceptions import DecryptionError, EncryptionError, InvalidKeyError
from crypto_lib.key_management import generate_key


def test_roundtrip_with_aad() -> None:
    key = generate_key(32)
    aad = b"tier:hot"
    result = encrypt_aad("hello world", key, aad)
    decrypted = decrypt_aad(result, key, aad)
    assert decrypted.to_string() == "hello world"


def test_wrong_aad_fails() -> None:
    key = generate_key(32)
    result = encrypt_aad("secret", key, b"tier:hot")
    with pytest.raises(DecryptionError):
        decrypt_aad(result, key, b"tier:cold")


def test_empty_aad_roundtrip() -> None:
    key = generate_key(32)
    result = encrypt_aad("data", key, b"")
    decrypted = decrypt_aad(result, key, b"")
    assert decrypted.to_string() == "data"


def test_bytes_input() -> None:
    key = generate_key(32)
    aad = b"context"
    result = encrypt_aad(b"\x00\x01\x02", key, aad)
    decrypted = decrypt_aad(result, key, aad)
    assert decrypted.plaintext == b"\x00\x01\x02"


def test_invalid_key_size_encrypt() -> None:
    with pytest.raises(InvalidKeyError):
        encrypt_aad("data", b"short", b"aad")


def test_invalid_key_size_decrypt() -> None:
    key = generate_key(32)
    result = encrypt_aad("data", key, b"aad")
    with pytest.raises(InvalidKeyError):
        decrypt_aad(result, b"short", b"aad")


def test_unsupported_algorithm() -> None:
    key = generate_key(32)
    with pytest.raises(EncryptionError):
        encrypt_aad("data", key, b"aad", algorithm="AES-128-GCM")


def test_base64_roundtrip() -> None:
    key = generate_key(32)
    aad = b"agent:did:euxis:agent-1"
    result = encrypt_aad("memory content", key, aad)
    encoded = result.to_base64()
    decrypted = decrypt_aad(encoded, key, aad)
    assert decrypted.to_string() == "memory content"


def test_unicode_data() -> None:
    key = generate_key(32)
    aad = b"tier:hot"
    result = encrypt_aad("こんにちは世界 🌍", key, aad)
    decrypted = decrypt_aad(result, key, aad)
    assert decrypted.to_string() == "こんにちは世界 🌍"
