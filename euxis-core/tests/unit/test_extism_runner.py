import json
import pytest
from unittest.mock import MagicMock, patch

from euxis_core.mesh.extism_runner import WasmExecutor

@pytest.fixture
def mock_extism():
    with patch("euxis_core.mesh.extism_runner.extism") as mock:
        with patch("euxis_core.mesh.extism_runner.HAS_EXTISM", True):
            yield mock

@pytest.fixture
def mock_no_extism():
    with patch("euxis_core.mesh.extism_runner.HAS_EXTISM", False):
        yield

def test_wasm_executor_init():
    executor = WasmExecutor("plugin.wasm")
    assert executor.plugin_path == "plugin.wasm"
    assert executor.wasi is True
    assert executor._plugin is None

def test_get_plugin_no_extism(mock_no_extism):
    executor = WasmExecutor("plugin.wasm")
    with pytest.raises(RuntimeError, match="Extism is not installed."):
        executor._get_plugin()

def test_extism_import_error():
    import sys
    with patch.dict(sys.modules, {"extism": None}):
        # Force a reload of the module to trigger ImportError block
        import importlib
        laid_out_module = importlib.import_module("euxis_core.mesh.extism_runner")
        importlib.reload(laid_out_module)
        assert laid_out_module.HAS_EXTISM is False
        assert laid_out_module.extism is None

def test_get_plugin_creates_plugin(mock_extism):
    executor = WasmExecutor("plugin.wasm")
    plugin = executor._get_plugin()
    
    # Assert extism.Plugin was called with manifest
    mock_extism.Plugin.assert_called_once()
    assert executor._plugin == mock_extism.Plugin.return_value
    assert plugin == mock_extism.Plugin.return_value

    # Call it again to hit the cached branch (lines 36->41)
    mock_extism.Plugin.reset_mock()
    plugin2 = executor._get_plugin()
    assert plugin2 == plugin
    mock_extism.Plugin.assert_not_called()

def test_call_success(mock_extism):
    executor = WasmExecutor("plugin.wasm")
    
    # Mock the plugin call returning serialized json bytes
    mock_plugin = MagicMock()
    mock_plugin.call.return_value = b'{"result": "success"}'
    mock_extism.Plugin.return_value = mock_plugin
    
    result = executor.call("test_func", {"input": "val"})
    
    assert result == {"result": "success"}
    mock_plugin.call.assert_called_once_with("test_func", b'{"input": "val"}')

def test_call_empty_output(mock_extism):
    executor = WasmExecutor("plugin.wasm")
    mock_plugin = MagicMock()
    mock_plugin.call.return_value = b""
    mock_extism.Plugin.return_value = mock_plugin
    
    result = executor.call("test_func", {"input": "val"})
    assert result == {}

def test_call_exception(mock_extism):
    executor = WasmExecutor("plugin.wasm")
    mock_plugin = MagicMock()
    mock_plugin.call.side_effect = Exception("Wasm failed")
    mock_extism.Plugin.return_value = mock_plugin
    
    with pytest.raises(RuntimeError, match="Wasm execution failed: Wasm failed"):
        executor.call("test_func", {"input": "val"})

def test_free(mock_extism):
    executor = WasmExecutor("plugin.wasm")
    plugin = executor._get_plugin()
    
    assert executor._plugin is not None
    executor.free()
    assert executor._plugin is None
    
    # Freeing again should be safe
    executor.free()
    assert executor._plugin is None
