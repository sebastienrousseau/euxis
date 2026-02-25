# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

import ast
import os
import re
from pathlib import Path
from typing import Dict, List, Set, Any
import asyncio

class WorkspaceMapper:
    """Recursively scans and maps cross-package dependencies."""

    def __init__(self, root_dir: str):
        self.root_dir = Path(root_dir).resolve()
        self.graph = {
            "nodes": {},
            "edges": []
        }
        self._ignore_dirs = {'.git', 'node_modules', '.venv', '__pycache__', 'dist', 'build', '.euxis', 'target'}
        
    async def scan_workspace(self):
        """Asynchronously crawl the workspace to avoid filesystem lag on WSL (9P)."""
        tasks = []
        for root, dirs, files in os.walk(self.root_dir):
            # In-place modification to respect ignores
            dirs[:] = [d for d in dirs if d not in self._ignore_dirs]
            
            for file in files:
                file_path = Path(root) / file
                if file.endswith('.py'):
                    tasks.append(self._parse_python(file_path))
                elif file.endswith('.sh') or file.endswith('.ts'):
                    tasks.append(self._parse_regex(file_path))
                    
        await asyncio.gather(*tasks)
        return self.graph

    async def _parse_python(self, file_path: Path):
        """Use AST to map Python relationships deeply."""
        rel_path = str(file_path.relative_to(self.root_dir))
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            tree = ast.parse(content, filename=str(file_path))
        except Exception:
            return

        node_data = {
            "type": "python",
            "classes": [],
            "functions": [],
            "imports": []
        }

        for node in ast.walk(tree):
            if isinstance(node, ast.ClassDef):
                node_data["classes"].append(node.name)
            elif isinstance(node, ast.FunctionDef) or isinstance(node, ast.AsyncFunctionDef):
                node_data["functions"].append(node.name)
            elif isinstance(node, ast.Import):
                for alias in node.names:
                    node_data["imports"].append(alias.name)
                    self._add_edge(rel_path, alias.name, "imports")
            elif isinstance(node, ast.ImportFrom):
                if node.module:
                    node_data["imports"].append(node.module)
                    self._add_edge(rel_path, node.module, "imports_from")

        self.graph["nodes"][rel_path] = node_data

    async def _parse_regex(self, file_path: Path):
        """Naive regex parsing for Shell and TS/JS files."""
        rel_path = str(file_path.relative_to(self.root_dir))
        node_data = {"type": "shell" if file_path.suffix == '.sh' else "typescript", "imports": []}
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except Exception:
            return

        if node_data["type"] == "shell":
            # Find sourced files
            sources = re.findall(r'^(?:source|\.)\s+([^\s]+)', content, re.MULTILINE)
            for src in sources:
                node_data["imports"].append(src)
                self._add_edge(rel_path, src, "sources")
        else:
            # Find TS imports
            imports = re.findall(r'import\s+.*?\s+from\s+[\'"](.*?)[\'"]', content)
            for imp in imports:
                node_data["imports"].append(imp)
                self._add_edge(rel_path, imp, "imports")

        self.graph["nodes"][rel_path] = node_data

    def _add_edge(self, source: str, target: str, relation: str):
        self.graph["edges"].append({
            "source": source,
            "target": target,
            "relation": relation
        })
