# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

from euxis_core.platform import RuntimePlatformAdapter
import euxis_core.platform.adapters as platform_adapters


def test_platform_name_is_normalized() -> None:
    platform_name = RuntimePlatformAdapter().platform_name()
    assert platform_name in {"linux", "macos", "windows", "wsl", "unknown"}


def test_platform_name_maps_darwin_to_macos(monkeypatch) -> None:
    monkeypatch.setattr(platform_adapters.py_platform, "system", lambda: "Darwin")
    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)
    assert RuntimePlatformAdapter().platform_name() == "macos"


def test_is_wsl_from_env(monkeypatch) -> None:
    monkeypatch.setenv("WSL_DISTRO_NAME", "Ubuntu-24.04")
    assert RuntimePlatformAdapter().platform_name() == "wsl"
    assert RuntimePlatformAdapter().is_wsl() is True
    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)


def test_is_wsl_false_without_env_or_proc(monkeypatch) -> None:
    class MissingProcPath:
        def exists(self) -> bool:
            return False

    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)
    monkeypatch.setattr(platform_adapters, "Path", lambda *_: MissingProcPath())
    assert RuntimePlatformAdapter().is_wsl() is False
