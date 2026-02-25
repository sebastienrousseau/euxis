# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

import json
from typing import Dict, Any

class GraphAdapter:
    """Formats the OmniGraph for different AI providers."""

    @staticmethod
    def format_for_provider(graph: Dict[str, Any], provider: str) -> str:
        """Routes to the correct formatting strategy."""
        provider = provider.lower()
        
        if provider in ["claude", "gemini", "anthropic"]:
            return GraphAdapter._format_system_prompt(graph)
        elif provider in ["openai", "gpt-4", "o1"]:
            return GraphAdapter._format_json(graph)
        elif provider in ["ollama", "llama", "local"]:
            return GraphAdapter._format_minimal(graph)
        else:
            return GraphAdapter._format_system_prompt(graph)

    @staticmethod
    def _format_system_prompt(graph: Dict[str, Any]) -> str:
        """High-density hierarchy for Claude/Gemini system prompts."""
        output = ["# EUXIS OMNIGRAPH: WORKSPACE CONTEXT\n"]
        
        for node, data in graph.get("nodes", {}).items():
            node_type = data.get("type", "unknown")
            output.append(f"## {node} ({node_type})")
            
            if data.get("classes"):
                output.append(f"- Classes: {', '.join(data['classes'])}")
            if data.get("functions"):
                output.append(f"- Functions: {', '.join(data['functions'])}")
                
        output.append("\n## RELATIONSHIPS")
        for edge in graph.get("edges", []):
            output.append(f"- {edge['source']} --[{edge['relation']}]--> {edge['target']}")
            
        return "\n".join(output)

    @staticmethod
    def _format_json(graph: Dict[str, Any]) -> str:
        """Raw JSON optimized for tools/file-search."""
        return json.dumps(graph, indent=2)

    @staticmethod
    def _format_minimal(graph: Dict[str, Any]) -> str:
        """Minimalist markdown for small context windows (Local LLMs)."""
        output = ["# Workspace Map\n"]
        for node in graph.get("nodes", {}).keys():
            output.append(f"- {node}")
        return "\n".join(output)
