import json
import pytest
import threading
import sys
from unittest.mock import MagicMock, patch
from pathlib import Path
import importlib

from euxis_core.mesh.extism_runner import WasmExecutor, PluginPool, AgentSerializer

@pytest.fixture(autouse=True)
def reset_plugin_pool():
    """Reset the PluginPool singleton before each test."""
    with PluginPool._lock:
        PluginPool._instance = None

@pytest.fixture
def mock_extism():
    with patch("euxis_core.mesh.extism_runner.extism") as mock:
        with patch("euxis_core.mesh.extism_runner.HAS_EXTISM", True):
            yield mock

@pytest.fixture
def mock_no_extism():
    with patch("euxis_core.mesh.extism_runner.HAS_EXTISM", False):
        yield

# --- AgentSerializer Tests ---

def test_serializer_json():
    data = {"key": "value"}
    serialized = AgentSerializer.serialize(data)
    assert isinstance(serialized, bytes)
    assert AgentSerializer.deserialize(serialized) == data

def test_serializer_empty():
    assert AgentSerializer.deserialize(b"") == {}

# --- PluginPool Tests ---

def test_plugin_pool_singleton():
    pool1 = PluginPool()
    pool2 = PluginPool()
    assert pool1 is pool2

def test_plugin_pool_acquire_new(mock_extism):
    pool = PluginPool()
    plugin = pool.acquire("test.wasm", True)
    mock_extism.Plugin.assert_called_once()
    assert plugin == mock_extism.Plugin.return_value

def test_plugin_pool_acquire_reuse(mock_extism):
    pool = PluginPool()
    mock_plugin = MagicMock()
    pool.release("test.wasm", mock_plugin)
    
    plugin = pool.acquire("test.wasm", True)
    assert plugin == mock_plugin
    mock_extism.Plugin.assert_not_called()

def test_plugin_pool_release_limit():
    pool = PluginPool()
    path = "test.wasm"
    plugins = [MagicMock() for _ in range(6)]
    
    for p in plugins:
        pool.release(path, p)
    
    assert len(pool._pool[path]) == 5

# --- WasmExecutor Tests ---

def test_wasm_executor_init():
    executor = WasmExecutor("plugin.wasm")
    assert executor.plugin_path == "plugin.wasm"
    assert executor.wasi is True
    assert isinstance(executor.pool, PluginPool)

def test_call_success(mock_extism):
    executor = WasmExecutor("plugin.wasm")
    mock_plugin = MagicMock()
    mock_plugin.call.return_value = json.dumps({"result": "success"}).encode("utf-8")
    
    with patch.object(PluginPool, "acquire", return_value=mock_plugin):
        with patch.object(PluginPool, "release") as mock_release:
            result = executor.call("test_func", {"input": "val"})
            assert result == {"result": "success"}
            mock_release.assert_called_once_with("plugin.wasm", mock_plugin)

def test_call_no_extism(mock_no_extism):
    executor = WasmExecutor("plugin.wasm")
    with pytest.raises(RuntimeError, match="Extism is not installed."):
        executor.call("test_func", {})

def test_call_exception(mock_extism):
    executor = WasmExecutor("plugin.wasm")
    mock_plugin = MagicMock()
    mock_plugin.call.side_effect = Exception("Wasm failed")
    
    with patch.object(PluginPool, "acquire", return_value=mock_plugin):
        with pytest.raises(RuntimeError, match="Wasm execution failed: Wasm failed"):
            executor.call("test_func", {})

def test_call_no_output(mock_extism):
    executor = WasmExecutor("plugin.wasm")
    mock_plugin = MagicMock()
    mock_plugin.call.return_value = None
    
    with patch.object(PluginPool, "acquire", return_value=mock_plugin):
        result = executor.call("test_func", {})
        assert result == {}

def test_extism_import_error():
    """Test the ImportError block in extism_runner."""
    with patch.dict(sys.modules, {"extism": None}):
        import euxis_core.mesh.extism_runner
        importlib.reload(euxis_core.mesh.extism_runner)
        assert euxis_core.mesh.extism_runner.HAS_EXTISM is False
        assert euxis_core.mesh.extism_runner.extism is None
    # Cleanup to restore state for other tests
    importlib.reload(euxis_core.mesh.extism_runner)
