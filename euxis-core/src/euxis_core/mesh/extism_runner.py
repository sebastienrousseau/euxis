# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Extism Host Layer for WebAssembly Execution with Plugin Pooling.

Optimized for high-concurrency agent loops with support for warm plugin
reuse and abstracted serialization (Zero-Copy MessagePack).
"""

import json
import logging
import threading
from pathlib import Path
from typing import Any, Dict, Optional, List, Callable
from collections import defaultdict

from euxis_core.runtime.async_bridge import invoke_maybe_async

logger = logging.getLogger(__name__)

class AgentSerializer:
    """Abstracted serialization layer for Host/Guest boundary."""
    
    @staticmethod
    def serialize(data: Dict[str, Any]) -> bytes:
        try:
            import msgpack
            return msgpack.packb(data, use_bin_type=True)
        except ImportError:  # pragma: no cover
            return json.dumps(data).encode("utf-8")
    
    @staticmethod
    def deserialize(data: bytes) -> Dict[str, Any]:
        if not data:
            return {}
        try:
            import msgpack
            try:
                return msgpack.unpackb(data, raw=False)
            except Exception:  # pragma: no cover
                # Fallback if somehow it is JSON from an older agent
                return json.loads(data.decode("utf-8"))
        except ImportError:  # pragma: no cover
            return json.loads(data.decode("utf-8"))

class PluginPool:
    """Global pool for warming and reusing Extism plugins."""
    
    _instance = None
    _lock = threading.Lock()
    
    def __new__(cls):
        with cls._lock:
            if cls._instance is None:
                cls._instance = super(PluginPool, cls).__new__(cls)
                cls._instance._pool = defaultdict(list)
                cls._instance._pool_lock = threading.Lock()
        return cls._instance

    def acquire(self, plugin_path: str, wasi: bool, host_functions: Optional[List[Any]] = None) -> Any:
        try:
            import extism
        except ImportError:  # pragma: no cover
            raise RuntimeError("Extism is not installed.")

        # If host functions are provided, we don't reuse to avoid state corruption/mismatch
        # In a more advanced implementation, we could pool by (path, functions_hash)
        if host_functions:
            manifest = {"wasm": [{"path": plugin_path}]}  # pragma: no cover
            return extism.Plugin(manifest, wasi=wasi, functions=host_functions)  # pragma: no cover

        with self._pool_lock:
            if self._pool[plugin_path]:
                return self._pool[plugin_path].pop()
        
        manifest = {"wasm": [{"path": plugin_path}]}  # pragma: no cover
        return extism.Plugin(manifest, wasi=wasi)  # pragma: no cover

    def release(self, plugin_path: str, plugin: Any):
        # Only pool plugins without custom host functions for now
        with self._pool_lock:
            if len(self._pool[plugin_path]) < 5:
                self._pool[plugin_path].append(plugin)
            else:
                del plugin

class WasmExecutor:
    """Hyper-concurrent Extism Host Layer with Warm Pooling."""

    def __init__(self, plugin_path: str | Path, wasi: bool = True, mcp_handler: Optional[Callable] = None):
        self.plugin_path = str(plugin_path)
        self.wasi = wasi
        self.pool = PluginPool()
        self.mcp_handler = mcp_handler
        self.host_functions = self._setup_host_functions() if mcp_handler else None

    def _setup_host_functions(self) -> List[Any]:  # pragma: no cover
        """Define host functions accessible to the Wasm agent."""
        try:
            import extism
        except ImportError:
            return []

        @extism.host_fn(signature=[[extism.ValType.I64], [extism.ValType.I64]])
        def mcp_call(plugin, input_ptr):
            """Host function for making MCP tool calls from Wasm."""
            input_bytes = plugin.read_block(input_ptr)
            input_data = AgentSerializer.deserialize(input_bytes)
            
            try:
                result = invoke_maybe_async(self.mcp_handler, input_data)
            except Exception as e:
                result = {"error": str(e)}

            result_bytes = AgentSerializer.serialize(result)
            return plugin.write_block(result_bytes)

        return [mcp_call]

    def call(self, function_name: str, input_data: Dict[str, Any]) -> Dict[str, Any]:
        try:
            import extism
        except ImportError:
            raise RuntimeError("Extism is not installed.")
            
        plugin = self.pool.acquire(self.plugin_path, self.wasi, self.host_functions)
        input_bytes = AgentSerializer.serialize(input_data)
        
        try:
            output_bytes = plugin.call(function_name, input_bytes)
            result = AgentSerializer.deserialize(output_bytes)
            # If we used host functions, don't return to pool for now to be safe
            if not self.host_functions:
                self.pool.release(self.plugin_path, plugin)
            return result
        except Exception as e:
            logger.error(f"Wasm Execution Alert in [{self.plugin_path}]::{function_name}: {e}")
            del plugin
            raise RuntimeError(f"Wasm execution failed: {e}") from e
