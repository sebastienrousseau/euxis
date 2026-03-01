from runtime.platform.contracts import PlatformInfo
from runtime.platform.factory import resolve_platform_ops


def test_runtime_platform_scaffold_contract_and_factory() -> None:
    info = PlatformInfo(platform="linux", runtime="generic")
    assert info.platform == "linux"
    ops = resolve_platform_ops()
    assert isinstance(ops.platform_info(), PlatformInfo)
    assert ops.path_separator() == "/"
