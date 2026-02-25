# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

import json
import logging

logger = logging.getLogger(__name__)

class TokenBudgeter:
    """Estimates and optimizes graph size for LLM context windows."""

    # Rough character-to-token ratio
    CHARS_PER_TOKEN = 4.0

    def __init__(self, max_tokens: int = 100000):
        self.max_tokens = max_tokens

    def estimate_tokens(self, text: str) -> int:
        """Naive but fast token estimation."""
        return int(len(text) / self.CHARS_PER_TOKEN)

    def optimize_graph(self, graph: dict, target_format: str) -> dict:
        """Prunes the graph if it exceeds the token budget."""
        # For this MVP, we perform a basic size check.
        # In a full implementation, we would recursively prune lowest-pagerank nodes.
        
        raw_json = json.dumps(graph)
        estimated_tokens = self.estimate_tokens(raw_json)
        
        logger.info(f"OmniGraph Estimated Token Cost: ~{estimated_tokens} tokens")
        
        if estimated_tokens > self.max_tokens:
            logger.warning(f"Graph exceeds budget ({estimated_tokens} > {self.max_tokens}). Pruning...")
            # Naive pruning: remove edges
            graph["edges"] = []
            
            re_estimated = self.estimate_tokens(json.dumps(graph))
            logger.info(f"Post-pruning cost: ~{re_estimated} tokens")
            
        return graph
