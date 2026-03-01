import pytest

from euxis_bridge.platform.factory import resolve_platform_ops
from euxis_bridge.platform.linux import LinuxOps
from euxis_bridge.platform.macos import MacOSOps
from euxis_bridge.platform.wsl import WSLOps


def test_linux_ops_methods() -> None:
    ops = LinuxOps()
    assert ops.platform_info().platform == "linux"
    assert ops.path_separator()
    assert ".euxis" in ops.join_workspace("abc")


def test_macos_ops_methods() -> None:
    ops = MacOSOps()
    assert ops.platform_info().platform == "darwin"
    assert ops.path_separator()
    assert ".euxis" in ops.join_workspace("abc")


def test_wsl_ops_methods() -> None:
    ops = WSLOps()
    assert ops.platform_info().runtime == "wsl"
    assert ops.path_separator()
    assert ".euxis" in ops.join_workspace("abc")


def test_factory_linux(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr("platform.system", lambda: "Linux")
    monkeypatch.setattr("platform.release", lambda: "6.0")
    monkeypatch.setattr("platform.version", lambda: "generic")
    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)
    assert isinstance(resolve_platform_ops(), LinuxOps)


def test_factory_macos(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr("platform.system", lambda: "Darwin")
    assert isinstance(resolve_platform_ops(), MacOSOps)


def test_factory_wsl(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr("platform.system", lambda: "Linux")
    monkeypatch.setattr("platform.release", lambda: "microsoft")
    monkeypatch.setattr("platform.version", lambda: "microsoft")
    assert isinstance(resolve_platform_ops(), WSLOps)


def test_factory_wsl_env_var(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr("platform.system", lambda: "Linux")
    monkeypatch.setattr("platform.release", lambda: "6.0")
    monkeypatch.setattr("platform.version", lambda: "generic")
    monkeypatch.setenv("WSL_DISTRO_NAME", "Ubuntu")
    assert isinstance(resolve_platform_ops(), WSLOps)


def test_factory_unsupported(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr("platform.system", lambda: "Windows")
    with pytest.raises(RuntimeError):
        resolve_platform_ops()
