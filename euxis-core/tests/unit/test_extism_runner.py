import json
import pytest
import threading
import sys
from unittest.mock import MagicMock, patch
from pathlib import Path
import importlib

import msgpack
from euxis_core.mesh.extism_runner import WasmExecutor, PluginPool, AgentSerializer

@pytest.fixture(autouse=True)
def reset_plugin_pool():
    """Reset the PluginPool singleton before each test."""
    with PluginPool._lock:
        PluginPool._instance = None

# --- AgentSerializer Tests ---

def test_serializer_msgpack():
    data = {"key": "value"}
    serialized = AgentSerializer.serialize(data)
    assert isinstance(serialized, bytes)
    assert AgentSerializer.deserialize(serialized) == data

def test_serializer_empty():
    assert AgentSerializer.deserialize(b"") == {}

def test_serializer_fallback():
    # Test JSON fallback when msgpack unpacking fails
    data = {"key": "value"}
    json_bytes = json.dumps(data).encode("utf-8")
    assert AgentSerializer.deserialize(json_bytes) == data

# --- PluginPool Tests ---

def test_plugin_pool_singleton():
    pool1 = PluginPool()
    pool2 = PluginPool()
    assert pool1 is pool2

def test_plugin_pool_acquire_new():
    with patch("extism.Plugin") as mock_plugin:
        pool = PluginPool()
        plugin = pool.acquire("test.wasm", True)
        mock_plugin.assert_called_once()
        assert plugin == mock_plugin.return_value

def test_plugin_pool_acquire_reuse():
    pool = PluginPool()
    mock_plugin = MagicMock()
    pool.release("test.wasm", mock_plugin)
    
    with patch("extism.Plugin") as mock_p:
        plugin = pool.acquire("test.wasm", True)
        assert plugin == mock_plugin
        mock_p.assert_not_called()

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

def test_call_success():
    executor = WasmExecutor("plugin.wasm")
    mock_plugin = MagicMock()
    mock_plugin.call.return_value = msgpack.packb({"result": "success"}, use_bin_type=True)
    
    with patch.object(PluginPool, "acquire", return_value=mock_plugin):
        with patch.object(PluginPool, "release") as mock_release:
            result = executor.call("test_func", {"input": "val"})
            assert result == {"result": "success"}
            mock_release.assert_called_once_with("plugin.wasm", mock_plugin)

def test_call_no_extism():
    with patch.dict(sys.modules, {"extism": None}):
        executor = WasmExecutor("plugin.wasm")
        # Need to patch the acquire method or mock extism inside it
        with patch.object(PluginPool, "acquire", side_effect=RuntimeError("Extism is not installed.")):
            with pytest.raises(RuntimeError, match="Extism is not installed."):
                executor.call("test_func", {})

def test_call_with_host_functions():
    async def mock_h(d): return {}
    executor = WasmExecutor("plugin.wasm", mcp_handler=mock_h)
    mock_plugin = MagicMock()
    mock_plugin.call.return_value = msgpack.packb({"ok": True})
    
    with patch.object(PluginPool, "acquire", return_value=mock_plugin):
        with patch.object(PluginPool, "release") as mock_release:
            executor.call("f", {})
            # Should NOT release to pool if host functions are used
            mock_release.assert_not_called()

def test_call_exception():
    executor = WasmExecutor("plugin.wasm")
    mock_plugin = MagicMock()
    mock_plugin.call.side_effect = Exception("Wasm failed")
    
    with patch.object(PluginPool, "acquire", return_value=mock_plugin):
        with pytest.raises(RuntimeError, match="Wasm execution failed: Wasm failed"):
            executor.call("test_func", {})
