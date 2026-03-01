import platform

import pytest

from euxis_inference.platform import factory
from euxis_inference.platform.linux import LinuxOps
from euxis_inference.platform.macos import MacOSOps
from euxis_inference.platform.wsl import WSLOps


def test_resolve_macos(monkeypatch) -> None:
    monkeypatch.setattr(platform, "system", lambda: "Darwin")
    assert isinstance(factory.resolve_platform_ops(), MacOSOps)


def test_resolve_linux(monkeypatch) -> None:
    monkeypatch.setattr(platform, "system", lambda: "Linux")
    monkeypatch.setattr(factory, "_is_wsl", lambda: False)
    assert isinstance(factory.resolve_platform_ops(), LinuxOps)


def test_resolve_wsl(monkeypatch) -> None:
    monkeypatch.setattr(platform, "system", lambda: "Linux")
    monkeypatch.setattr(factory, "_is_wsl", lambda: True)
    assert isinstance(factory.resolve_platform_ops(), WSLOps)


def test_is_wsl_detects_env(monkeypatch) -> None:
    monkeypatch.setenv("WSL_DISTRO_NAME", "Ubuntu")
    assert factory._is_wsl() is True


def test_is_wsl_detects_release_signature(monkeypatch) -> None:
    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)
    monkeypatch.setattr(platform, "release", lambda: "5.15.153.1-microsoft-standard-WSL2")
    monkeypatch.setattr(platform, "version", lambda: "#1 SMP")
    assert factory._is_wsl() is True


def test_is_wsl_false_when_no_markers(monkeypatch) -> None:
    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)
    monkeypatch.setattr(platform, "release", lambda: "6.10.0-generic")
    monkeypatch.setattr(platform, "version", lambda: "#1 SMP")
    assert factory._is_wsl() is False


def test_resolve_unsupported_platform(monkeypatch) -> None:
    monkeypatch.setattr(platform, "system", lambda: "Windows")
    with pytest.raises(RuntimeError, match="unsupported platform"):
        factory.resolve_platform_ops()
