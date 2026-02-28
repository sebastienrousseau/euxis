# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""FinOps Router for cost-aware model provider selection."""

from __future__ import annotations

import logging
from dataclasses import dataclass
from typing import Any, Dict, List, Optional

LOGGER = logging.getLogger("euxis.core.router")

@dataclass
class ProviderMetrics:
    name: str
    cost_per_1k_tokens: float # in USD
    avg_latency_ms: int
    reliability_score: float # 0.0 to 1.0

class FinOpsRouter:
    """Intelligent router that balances cost, speed, and reliability."""

    def __init__(self, budget_limit: float = 10.0):
        self.budget_limit = budget_limit
        self.current_spend = 0.0
        # Default 2026 model pricing / performance stats
        self.providers = [
            ProviderMetrics("ollama", 0.000, 150, 0.95),    # Local Edge AI
            ProviderMetrics("groq", 0.0001, 50, 0.98),      # Fast LPU
            ProviderMetrics("anthropic", 0.015, 800, 0.99), # High reasoning
            ProviderMetrics("openai", 0.010, 600, 0.99),    # Balanced
        ]

    def select_provider(self, task_complexity: str, priority: str = "balanced") -> str:
        """Select the best provider for a given task and priority."""
        LOGGER.info(f"Routing task (complexity: {task_complexity}, priority: {priority})")
        
        if task_complexity == "low":
            # Always prefer local for low complexity
            return "ollama"
            
        if priority == "speed":
            # Sort by latency
            sorted_providers = sorted(self.providers, key=lambda p: p.avg_latency_ms)
            return sorted_providers[0].name
            
        if priority == "cost":
            # Sort by cost
            sorted_providers = sorted(self.providers, key=lambda p: p.cost_per_1k_tokens)
            return sorted_providers[0].name
            
        # Default: Weighted score balancing reliability, cost, and latency.
        best_provider = "ollama"
        best_score = -100.0
        
        for p in self.providers:
            # Normalized values for scoring
            cost_factor = p.cost_per_1k_tokens * 100 # Scale up for significance
            latency_factor = p.avg_latency_ms / 1000
            
            score = (p.reliability_score * 10.0) - (cost_factor * 5.0) - (latency_factor * 2.0)
            
            if score > best_score:
                best_score = score
                best_provider = p.name
                
        LOGGER.info(f"FinOps Router selected: {best_provider} (score: {best_score:.2f})")
        return best_provider

    def track_usage(self, provider_name: str, tokens: int):
        """Update spend tracking."""
        provider = next((p for p in self.providers if p.name == provider_name), None)
        if provider:
            cost = (tokens / 1000) * provider.cost_per_1k_tokens
            self.current_spend += cost
            LOGGER.info(f"Current session spend: ${self.current_spend:.4f}")
