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
from typing import Any, Dict, Optional, List
from collections import defaultdict

logger = logging.getLogger(__name__)

class AgentSerializer:
    """Abstracted serialization layer for Host/Guest boundary."""
    
    @staticmethod
    def serialize(data: Dict[str, Any]) -> bytes:
        try:
            import msgpack
            return msgpack.packb(data, use_bin_type=True)
        except ImportError:
            return json.dumps(data).encode("utf-8")
    
    @staticmethod
    def deserialize(data: bytes) -> Dict[str, Any]:
        if not data:
            return {}
        try:
            import msgpack
            try:
                return msgpack.unpackb(data, raw=False)
            except Exception:
                # Fallback if somehow it is JSON from an older agent
                return json.loads(data.decode("utf-8"))
        except ImportError:
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

    def acquire(self, plugin_path: str, wasi: bool) -> Any:
        try:
            import extism
        except ImportError:
            raise RuntimeError("Extism is not installed.")

        with self._pool_lock:
            if self._pool[plugin_path]:
                return self._pool[plugin_path].pop()
        
        manifest = {"wasm": [{"path": plugin_path}]}
        return extism.Plugin(manifest, wasi=wasi)

    def release(self, plugin_path: str, plugin: Any):
        with self._pool_lock:
            if len(self._pool[plugin_path]) < 5:
                self._pool[plugin_path].append(plugin)
            else:
                del plugin

class WasmExecutor:
    """Hyper-concurrent Extism Host Layer with Warm Pooling."""

    def __init__(self, plugin_path: str | Path, wasi: bool = True):
        self.plugin_path = str(plugin_path)
        self.wasi = wasi
        self.pool = PluginPool()

    def call(self, function_name: str, input_data: Dict[str, Any]) -> Dict[str, Any]:
        try:
            import extism
        except ImportError:
            raise RuntimeError("Extism is not installed.")
            
        plugin = self.pool.acquire(self.plugin_path, self.wasi)
        input_bytes = AgentSerializer.serialize(input_data)
        
        try:
            output_bytes = plugin.call(function_name, input_bytes)
            result = AgentSerializer.deserialize(output_bytes)
            self.pool.release(self.plugin_path, plugin)
            return result
        except Exception as e:
            logger.error(f"Wasm Execution Alert in [{self.plugin_path}]::{function_name}: {e}")
            del plugin
            raise RuntimeError(f"Wasm execution failed: {e}") from e
