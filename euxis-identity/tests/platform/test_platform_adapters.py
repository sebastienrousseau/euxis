from euxis_identity.platform.linux import LinuxOps
from euxis_identity.platform.macos import MacOSOps
from euxis_identity.platform.wsl import WSLOps


def test_linux_adapter_contract() -> None:
    ops = LinuxOps()
    info = ops.platform_info()
    assert info.platform == "linux"
    assert info.runtime == "native"
    assert ops.path_separator() == "/"
    assert ops.join_workspace("projects/demo") == "~/.euxis/projects/demo"


def test_macos_adapter_contract() -> None:
    ops = MacOSOps()
    info = ops.platform_info()
    assert info.platform == "macos"
    assert info.runtime == "native"
    assert ops.path_separator() == "/"
    assert ops.join_workspace("projects/demo") == "~/.euxis/projects/demo"


def test_wsl_adapter_contract() -> None:
    ops = WSLOps()
    info = ops.platform_info()
    assert info.platform == "linux"
    assert info.runtime == "wsl"
    assert ops.path_separator() == "/"
    assert "/mnt/c/Users/" in ops.join_workspace("projects/demo")
