# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Extism Host Layer for WebAssembly Execution.

This module provides a memory-safe execution environment for agent
business logic wrapped inside specialized WebAssembly (Wasm) plugins.
"""

import json
import logging
from pathlib import Path
from typing import Any, Dict, Optional

try:
    import extism
    HAS_EXTISM = True
except ImportError:
    extism = None
    HAS_EXTISM = False

logger = logging.getLogger(__name__)

class WasmExecutor:
    """Hyper-concurrent Extism Host Layer for executing WebAssembly plugins."""

    def __init__(self, plugin_path: str | Path, wasi: bool = True):
        self.plugin_path = str(plugin_path)
        self.wasi = wasi
        self._plugin: Optional[Any] = None

    def _get_plugin(self) -> Any:
        if not HAS_EXTISM:
            raise RuntimeError("Extism is not installed.")
            
        if self._plugin is None:
            # Resolves the Wasm binary directly from disk.
            # In Phase 2b we will expand this to support URL/Registry pulls.
            manifest = {"wasm": [{"path": self.plugin_path}]}
            self._plugin = extism.Plugin(manifest, wasi=self.wasi)
        return self._plugin

    def call(self, function_name: str, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """Execute a function inside the Wasm sandbox deterministically.
        
        Args:
            function_name: The exported Wasm function to invoke.
            input_data: A JSON-serializable dictionary to pass as input.
            
        Returns:
            The output JSON payload parsed back into a dictionary.
        """
        plugin = self._get_plugin()
        
        # Serialize python primitives crossing the Extism Host/Guest boundary
        input_bytes = json.dumps(input_data).encode("utf-8")
        
        try:
            output_bytes = plugin.call(function_name, input_bytes)
            if not output_bytes:
                return {}
            return json.loads(output_bytes.decode("utf-8"))
        except Exception as e:
            logger.error(f"Wasm Execution Alert in [{self.plugin_path}]::{function_name}: {e}")
            raise RuntimeError(f"Wasm execution failed: {e}") from e

    def free(self):
        """Releases the WebAssembly linear memory cleanly."""
        if self._plugin is not None:
            del self._plugin
            self._plugin = None
