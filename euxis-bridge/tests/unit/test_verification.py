"""Tests for skill signature verification."""

from pathlib import Path
from unittest.mock import patch

import pytest

from euxis_bridge.core.verification import (
    SkillVerificationError,
    _decode_signature,
    require_verified,
    verify_skill_signature,
)


def test_missing_signature_file(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("console.log('hi')", encoding="utf-8")
    pub = tmp_path / "pub.key"
    pub.write_text("fake-key", encoding="utf-8")

    assert verify_skill_signature(entry, str(pub)) is False


def test_missing_public_key_file(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("console.log('hi')", encoding="utf-8")
    sig = tmp_path / "index.js.sig"
    sig.write_bytes(b"fakesig")

    assert verify_skill_signature(entry, "/nonexistent/pub.key") is False


def test_require_verified_raises(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("console.log('hi')", encoding="utf-8")

    with pytest.raises(SkillVerificationError, match="signature verification failed"):
        require_verified(entry, "/nonexistent/pub.key")


def test_decode_signature_raw() -> None:
    assert _decode_signature(b"raw", "raw") == b"raw"


def test_decode_signature_base64() -> None:
    import base64

    encoded = base64.b64encode(b"hello")
    assert _decode_signature(encoded, "base64") == b"hello"


def test_decode_signature_hex() -> None:
    import binascii

    encoded = binascii.hexlify(b"hello")
    assert _decode_signature(encoded, "hex") == b"hello"


def test_decode_signature_unsupported() -> None:
    with pytest.raises(SkillVerificationError, match="unsupported"):
        _decode_signature(b"x", "unknown_enc")


def test_verify_with_mock_rust_backend(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("console.log('ok')", encoding="utf-8")
    sig = tmp_path / "index.js.sig"

    import base64

    sig.write_bytes(base64.b64encode(b"fakesig"))

    pub = tmp_path / "pub.key"
    pub.write_bytes(base64.b64encode(b"fakepubkey"))

    mock_crypto = type("MockCrypto", (), {"ed25519_verify": staticmethod(lambda pk, msg, s: True)})()
    with patch("euxis_bridge.core.verification._load_crypto", return_value=(mock_crypto, None)):
        assert verify_skill_signature(entry, str(pub)) is True


def test_verify_with_mock_rust_backend_fail(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("bad", encoding="utf-8")
    sig = tmp_path / "index.js.sig"

    import base64

    sig.write_bytes(base64.b64encode(b"fakesig"))
    pub = tmp_path / "pub.key"
    pub.write_bytes(base64.b64encode(b"fakepubkey"))

    mock_crypto = type("MockCrypto", (), {"ed25519_verify": staticmethod(lambda pk, msg, s: False)})()
    with patch("euxis_bridge.core.verification._load_crypto", return_value=(mock_crypto, None)):
        assert verify_skill_signature(entry, str(pub)) is False


def test_verify_no_crypto_backend_raises(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("test", encoding="utf-8")
    sig = tmp_path / "index.js.sig"
    sig.write_bytes(b"fakesig")
    pub = tmp_path / "pub.key"
    pub.write_bytes(b"fakepub")

    with patch("euxis_bridge.core.verification._load_crypto", return_value=(None, None)):
        with pytest.raises(SkillVerificationError, match="no crypto backend"):
            verify_skill_signature(entry, str(pub), sig_encoding="raw")


def test_load_crypto_no_backends() -> None:
    """_load_crypto returns (None, None) when neither backend is installed."""
    from euxis_bridge.core.verification import _load_crypto

    with patch.dict("sys.modules", {"crypto_lib_rs": None, "cryptography": None,
                                     "cryptography.hazmat.primitives": None,
                                     "cryptography.hazmat.primitives.serialization": None}):
        result = _load_crypto()
    assert result == (None, None)


def test_load_crypto_with_rust_backend() -> None:
    """_load_crypto returns (module, None) when crypto_lib_rs is available."""
    import types

    from euxis_bridge.core.verification import _load_crypto

    fake_rs = types.ModuleType("crypto_lib_rs")
    with patch.dict("sys.modules", {"crypto_lib_rs": fake_rs}):
        rs, ser = _load_crypto()
    assert rs is fake_rs
    assert ser is None


def test_load_crypto_with_cryptography_fallback() -> None:
    """_load_crypto returns (None, serialization) when only cryptography is available."""
    import types

    from euxis_bridge.core.verification import _load_crypto

    fake_ser = types.ModuleType("serialization")
    fake_ser.load_pem_public_key = lambda _: None  # type: ignore[attr-defined]

    with patch.dict(
        "sys.modules",
        {"crypto_lib_rs": None,
         "cryptography": types.ModuleType("cryptography"),
         "cryptography.hazmat": types.ModuleType("cryptography.hazmat"),
         "cryptography.hazmat.primitives": types.ModuleType("cryptography.hazmat.primitives"),
         "cryptography.hazmat.primitives.serialization": fake_ser},
    ):
        rs, ser = _load_crypto()
    assert rs is None
    assert ser is fake_ser


def test_verify_serialization_fallback_success(tmp_path: Path) -> None:
    """verify_skill_signature works via the serialization (cryptography) path."""
    import types

    entry = tmp_path / "index.js"
    entry.write_text("hello", encoding="utf-8")
    sig = tmp_path / "index.js.sig"
    sig.write_bytes(b"rawsig")
    pub = tmp_path / "pub.pem"
    pub.write_bytes(b"PEM-KEY")

    mock_pubkey = type("MockKey", (), {"verify": lambda self, sig, msg: None})()

    fake_ser = types.ModuleType("serialization")
    fake_ser.load_pem_public_key = lambda _: mock_pubkey  # type: ignore[attr-defined]

    fake_ed = types.ModuleType("ed25519")
    fake_ed.Ed25519PublicKey = type(mock_pubkey)  # type: ignore[attr-defined]

    with patch("euxis_bridge.core.verification._load_crypto", return_value=(None, fake_ser)):
        with patch.dict("sys.modules", {
            "cryptography.hazmat.primitives.asymmetric.ed25519": fake_ed,
        }):
            assert verify_skill_signature(entry, str(pub), sig_encoding="raw") is True


def test_verify_serialization_fallback_wrong_key_type(tmp_path: Path) -> None:
    """verify_skill_signature returns False if key is not Ed25519."""
    import types

    entry = tmp_path / "index.js"
    entry.write_text("hello", encoding="utf-8")
    sig = tmp_path / "index.js.sig"
    sig.write_bytes(b"rawsig")
    pub = tmp_path / "pub.pem"
    pub.write_bytes(b"PEM-KEY")

    # Return a key that isn't an Ed25519PublicKey instance
    mock_pubkey = object()

    fake_ser = types.ModuleType("serialization")
    fake_ser.load_pem_public_key = lambda _: mock_pubkey  # type: ignore[attr-defined]

    fake_ed = types.ModuleType("ed25519")
    fake_ed.Ed25519PublicKey = type("Ed25519PublicKey", (), {})  # type: ignore[attr-defined]

    with patch("euxis_bridge.core.verification._load_crypto", return_value=(None, fake_ser)):
        with patch.dict("sys.modules", {
            "cryptography.hazmat.primitives.asymmetric.ed25519": fake_ed,
        }):
            assert verify_skill_signature(entry, str(pub), sig_encoding="raw") is False


def test_verify_serialization_fallback_invalid_sig(tmp_path: Path) -> None:
    """verify_skill_signature returns False when signature doesn't match (cryptography path)."""
    import types

    entry = tmp_path / "index.js"
    entry.write_text("hello", encoding="utf-8")
    sig = tmp_path / "index.js.sig"
    sig.write_bytes(b"badsig")
    pub = tmp_path / "pub.pem"
    pub.write_bytes(b"PEM-KEY")

    mock_pubkey = type("MockKey", (), {
        "verify": lambda self, sig, msg: (_ for _ in ()).throw(Exception("bad sig"))
    })()

    fake_ser = types.ModuleType("serialization")
    fake_ser.load_pem_public_key = lambda _: mock_pubkey  # type: ignore[attr-defined]

    fake_ed = types.ModuleType("ed25519")
    fake_ed.Ed25519PublicKey = type(mock_pubkey)  # type: ignore[attr-defined]

    with patch("euxis_bridge.core.verification._load_crypto", return_value=(None, fake_ser)):
        with patch.dict("sys.modules", {
            "cryptography.hazmat.primitives.asymmetric.ed25519": fake_ed,
        }):
            assert verify_skill_signature(entry, str(pub), sig_encoding="raw") is False


def test_require_verified_success(tmp_path: Path) -> None:
    """require_verified returns None (no exception) when verification passes."""
    entry = tmp_path / "index.js"
    entry.write_text("ok", encoding="utf-8")

    with patch(
        "euxis_bridge.core.verification.verify_skill_signature", return_value=True
    ):
        result = require_verified(entry, "/fake/pub.key")
    assert result is None
