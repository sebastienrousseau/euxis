import importlib
from pathlib import Path
import sys


def test_gateway_module_imports():
    src_dir = Path(__file__).resolve().parents[1] / "src"
    if str(src_dir) not in sys.path:
        sys.path.insert(0, str(src_dir))
    session_import = importlib.import_module("gateway.session_import")
    session_export = importlib.import_module("gateway.session_export")
    assert session_import is not None
    assert session_export is not None
