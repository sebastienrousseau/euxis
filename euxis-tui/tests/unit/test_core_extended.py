# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Extended unit tests for core modules to reach >= 95% coverage.

Covers missing paths in:
- tui/core/services.py  (AES256GCM decrypt unavailable, encrypt/decrypt success/error)
- tui/core/__init__.py  (_resolve_euxis_home: env, walk-up, fallback)
- tui/__init__.py       (main() entry point, __version__)
"""

from __future__ import annotations

import importlib
import os
import sys
from pathlib import Path
from unittest.mock import MagicMock, Mock, patch

import pytest


# ---------------------------------------------------------------------------
# tui/core/__init__.py -- _resolve_euxis_home and EUXIS_HOME
# ---------------------------------------------------------------------------

class TestResolveEuxisHome:
    """Tests for _resolve_euxis_home() resolution order."""

    def _reload_core_init(self):
        """Force re-import of tui.core to re-execute _resolve_euxis_home."""
        sys.modules.pop("tui.core", None)
        import tui.core
        return tui.core

    def test_env_variable_takes_priority(self, tmp_path):
        """When EUXIS_HOME is set, that path is returned directly."""
        custom_home = str(tmp_path / "custom-euxis")
        with patch.dict("os.environ", {"EUXIS_HOME": custom_home}):
            mod = self._reload_core_init()
            assert mod.EUXIS_HOME == Path(custom_home)

    def test_walk_up_finds_registry_json(self, tmp_path):
        """When cwd ancestry contains euxis-core/agents/registry.json, use that."""
        marker_dir = tmp_path / "project" / "euxis-core" / "agents"
        marker_dir.mkdir(parents=True)
        (marker_dir / "registry.json").write_text("{}")

        work_dir = tmp_path / "project" / "sub" / "deep"
        work_dir.mkdir(parents=True)

        env = os.environ.copy()
        env.pop("EUXIS_HOME", None)
        with patch.dict("os.environ", env, clear=True), \
             patch("pathlib.Path.cwd", return_value=work_dir):
            mod = self._reload_core_init()
            assert mod.EUXIS_HOME == tmp_path / "project"

    def test_fallback_to_home_dot_euxis(self, tmp_path):
        """When no env and no marker found, fall back to ~/.euxis."""
        isolated_dir = tmp_path / "isolated" / "deep"
        isolated_dir.mkdir(parents=True)

        env = os.environ.copy()
        env.pop("EUXIS_HOME", None)
        with patch.dict("os.environ", env, clear=True), \
             patch("pathlib.Path.cwd", return_value=isolated_dir), \
             patch("pathlib.Path.home", return_value=tmp_path):
            mod = self._reload_core_init()
            assert mod.EUXIS_HOME == tmp_path / ".euxis"

    def test_cwd_itself_has_marker(self, tmp_path):
        """When cwd itself contains euxis-core/agents/registry.json."""
        marker_dir = tmp_path / "euxis-core" / "agents"
        marker_dir.mkdir(parents=True)
        (marker_dir / "registry.json").write_text("{}")

        env = os.environ.copy()
        env.pop("EUXIS_HOME", None)
        with patch.dict("os.environ", env, clear=True), \
             patch("pathlib.Path.cwd", return_value=tmp_path):
            mod = self._reload_core_init()
            assert mod.EUXIS_HOME == tmp_path


# ---------------------------------------------------------------------------
# tui/__init__.py -- __version__ and main()
# ---------------------------------------------------------------------------

class TestTuiInitModule:
    """Tests for tui/__init__.py."""

    def test_version_is_string(self):
        """__version__ should be a non-empty string."""
        from tui import __version__
        assert isinstance(__version__, str)
        assert len(__version__) > 0

    def test_version_format(self):
        """__version__ follows semver-like x.y.z format."""
        from tui import __version__
        parts = __version__.split(".")
        assert len(parts) == 3
        for part in parts:
            assert part.isdigit()

    @patch("tui.app.EuxisApp")
    def test_main_creates_and_runs_app(self, mock_app_cls):
        """main() should instantiate EuxisApp and call run()."""
        mock_instance = Mock()
        mock_app_cls.return_value = mock_instance

        from tui import main
        main()

        mock_app_cls.assert_called_once()
        mock_instance.run.assert_called_once()

    @patch("tui.app.EuxisApp")
    def test_main_app_run_called_with_no_args(self, mock_app_cls):
        """main() calls app.run() with no arguments."""
        mock_instance = Mock()
        mock_app_cls.return_value = mock_instance

        from tui import main
        main()

        mock_instance.run.assert_called_once_with()

    @patch("tui.app.EuxisApp")
    def test_main_does_not_return_value(self, mock_app_cls):
        """main() should return None (no explicit return value)."""
        mock_app_cls.return_value = Mock()

        from tui import main
        result = main()

        assert result is None


# ---------------------------------------------------------------------------
# Helper to reload tui.core.services with mocked crypto_bridge
# ---------------------------------------------------------------------------

def _fresh_services_module():
    """Remove tui.core.services from cache and re-import it.

    Must be called INSIDE a ``patch.dict("sys.modules", ...)`` block
    that supplies the mocked ``shared`` / ``shared.crypto_bridge``.
    """
    sys.modules.pop("tui.core.services", None)
    import tui.core.services as svc  # noqa: WPS433
    return svc


# ---------------------------------------------------------------------------
# tui/core/services.py -- AES256GCMAlgorithm coverage gaps
# ---------------------------------------------------------------------------

class TestAES256GCMDecryptUnavailable:
    """Test AES256GCMAlgorithm.decrypt when crypto_lib is NOT available."""

    def test_decrypt_raises_when_crypto_unavailable(self):
        """decrypt() should raise RuntimeError when CRYPTO_LIB_AVAILABLE is False."""
        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: False,
            ),
        }):
            svc = _fresh_services_module()
            algo = svc.AES256GCMAlgorithm()

            with pytest.raises(RuntimeError, match="not available"):
                algo.decrypt(b"encrypted-data", b"k" * 32)


class TestAES256GCMEncryptSuccess:
    """Test AES256GCMAlgorithm.encrypt success path."""

    def test_encrypt_returns_base64_bytes(self):
        """encrypt() should call crypto_encrypt and return base64 bytes."""
        mock_result = MagicMock()
        mock_result.to_base64.return_value = "base64encodeddata"
        mock_encrypt_fn = MagicMock(return_value=mock_result)

        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=mock_encrypt_fn,
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()
            algo = svc.AES256GCMAlgorithm()

            result = algo.encrypt(b"hello world", b"k" * 32)

            assert result == b"base64encodeddata"
            mock_encrypt_fn.assert_called_once_with(
                b"hello world", b"k" * 32, "AES-256-GCM"
            )
            mock_result.to_base64.assert_called_once()


class TestAES256GCMEncryptError:
    """Test AES256GCMAlgorithm.encrypt error path."""

    def test_encrypt_wraps_crypto_error_in_runtime_error(self):
        """encrypt() should wrap CryptoError/InvalidKeyError in RuntimeError."""
        mock_encrypt_fn = MagicMock(side_effect=Exception("bad key"))

        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=mock_encrypt_fn,
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()
            algo = svc.AES256GCMAlgorithm()

            with pytest.raises(RuntimeError, match="encryption failed"):
                algo.encrypt(b"data", b"k" * 32)


class TestAES256GCMDecryptSuccess:
    """Test AES256GCMAlgorithm.decrypt success path."""

    def test_decrypt_returns_plaintext_bytes(self):
        """decrypt() should call crypto_decrypt and return plaintext."""
        mock_dec_result = MagicMock()
        mock_dec_result.plaintext = b"decrypted plaintext"
        mock_decrypt_fn = MagicMock(return_value=mock_dec_result)

        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=mock_decrypt_fn,
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()
            algo = svc.AES256GCMAlgorithm()

            result = algo.decrypt(b"base64encodeddata", b"k" * 32)

            assert result == b"decrypted plaintext"
            mock_decrypt_fn.assert_called_once_with("base64encodeddata", b"k" * 32)


class TestAES256GCMDecryptError:
    """Test AES256GCMAlgorithm.decrypt error path."""

    def test_decrypt_wraps_crypto_error_in_runtime_error(self):
        """decrypt() should wrap CryptoError/DecryptionError in RuntimeError."""
        mock_decrypt_fn = MagicMock(side_effect=Exception("bad ciphertext"))

        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=mock_decrypt_fn,
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()
            algo = svc.AES256GCMAlgorithm()

            with pytest.raises(RuntimeError, match="decryption failed"):
                algo.decrypt(b"bad-data", b"k" * 32)

    def test_decrypt_handles_unicode_decode_error(self):
        """decrypt() should handle UnicodeDecodeError from encrypted_data.decode()."""
        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()
            algo = svc.AES256GCMAlgorithm()

            # Raw bytes that cannot be decoded as UTF-8
            bad_bytes = b"\xff\xfe\x80\x81" * 8
            with pytest.raises(RuntimeError, match="decryption failed"):
                algo.decrypt(bad_bytes, b"k" * 32)


class TestCreateDefaultCryptoServiceWithoutCrypto:
    """Test create_default_crypto_service when crypto_lib is unavailable."""

    def test_without_crypto_defaults_to_xor(self):
        """When CRYPTO_LIB_AVAILABLE is False, default algo is simple-xor."""
        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: False,
            ),
        }):
            svc = _fresh_services_module()
            crypto_service = svc.create_default_crypto_service()

            algos = crypto_service.list_available_algorithms()
            assert "simple-xor" in algos
            assert "sha256" in algos
            assert "aes-256-gcm" not in algos


class TestSetupServiceContainerWithoutCrypto:
    """Test setup_service_container when crypto_lib is unavailable."""

    def test_container_works_without_crypto(self):
        """setup_service_container works even when crypto_lib is missing."""
        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: False,
            ),
        }):
            svc = _fresh_services_module()
            container = svc.setup_service_container()

            registry = container.get(svc.AlgorithmRegistry)
            assert "simple-xor" in registry.list_algorithms()
            assert "aes-256-gcm" not in registry.list_algorithms()

            crypto = container.get(svc.CryptoService)
            assert crypto is not None
            assert "simple-xor" in crypto.list_available_algorithms()


class TestCryptoServiceDecryptWithSpecificAlgorithm:
    """Additional CryptoService coverage: decrypt with explicit algorithm arg."""

    def test_decrypt_with_explicit_algorithm(self):
        """CryptoService.decrypt() with an explicit algorithm name."""
        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()

            registry = svc.AlgorithmRegistry()
            xor = svc.SimpleXORAlgorithm()
            xor_info = svc.AlgorithmInfo("simple-xor", 32, "XOR", "symmetric")
            registry.register(xor, xor_info)

            sha = svc.SHA256Algorithm()
            sha_info = svc.AlgorithmInfo("sha256", 0, "SHA256", "hash")
            registry.register(sha, sha_info)

            km = svc.InMemoryKeyManager()
            km.set_algorithm_registry(registry)

            service = svc.CryptoService(registry, km, "simple-xor")
            service.generate_and_store_key("test-key", "simple-xor")

            data = b"secret data here!"
            encrypted = service.encrypt(data, "test-key", "simple-xor")
            decrypted = service.decrypt(encrypted, "test-key", "simple-xor")
            assert decrypted == data


class TestCryptoServiceGenerateAndStoreKeyDefault:
    """Test generate_and_store_key using default algorithm (no explicit algo)."""

    def test_generate_key_with_default_algorithm(self):
        """generate_and_store_key with no algorithm uses default."""
        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()

            registry = svc.AlgorithmRegistry()
            xor = svc.SimpleXORAlgorithm()
            xor_info = svc.AlgorithmInfo("simple-xor", 32, "XOR", "symmetric")
            registry.register(xor, xor_info)

            km = svc.InMemoryKeyManager()
            km.set_algorithm_registry(registry)

            service = svc.CryptoService(registry, km, "simple-xor")
            key_id = service.generate_and_store_key("auto-key")

            assert key_id == "auto-key"
            key = km.retrieve_key("auto-key")
            assert len(key) == 32


class TestCryptoServiceHashDefault:
    """Test CryptoService.hash with default algorithm parameter."""

    def test_hash_uses_sha256_by_default(self):
        """hash() defaults to sha256 when no algorithm specified."""
        import hashlib

        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()

            registry = svc.AlgorithmRegistry()
            xor = svc.SimpleXORAlgorithm()
            xor_info = svc.AlgorithmInfo("simple-xor", 32, "XOR", "symmetric")
            registry.register(xor, xor_info)

            sha = svc.SHA256Algorithm()
            sha_info = svc.AlgorithmInfo("sha256", 0, "SHA256", "hash")
            registry.register(sha, sha_info)

            km = svc.InMemoryKeyManager()
            km.set_algorithm_registry(registry)

            service = svc.CryptoService(registry, km, "simple-xor")
            result = service.hash(b"test data")
            assert result == hashlib.sha256(b"test data").digest()


class TestServiceContainerSingletonPriority:
    """Test ServiceContainer singleton-over-transient priority."""

    def test_singleton_takes_priority_over_transient(self):
        """If both singleton and transient exist, singleton wins."""
        with patch.dict("sys.modules", {
            "shared": MagicMock(),
            "shared.crypto_bridge": MagicMock(
                encrypt=MagicMock(),
                decrypt=MagicMock(),
                crypto_errors=lambda: (Exception, Exception, Exception),
                is_available=lambda: True,
            ),
        }):
            svc = _fresh_services_module()
            container = svc.ServiceContainer()

            singleton_val = {"type": "singleton"}
            container.register_singleton(dict, singleton_val)
            container.register_transient(dict, lambda: {"type": "transient"})

            result = container.get(dict)
            assert result is singleton_val
            assert result["type"] == "singleton"
