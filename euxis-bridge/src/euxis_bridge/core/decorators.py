"""Decorator helpers for OpenClaw-compatible skill wrappers."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any


def openclaw_skill(name: str, slug: str) -> Callable[[Callable[..., Any]], Callable[..., Any]]:
    """Annotate a Python function as a bridge-compatible skill wrapper."""

    def decorator(func: Callable[..., Any]) -> Callable[..., Any]:
        setattr(func, "_openclaw_skill", {"name": name, "slug": slug})
        return func

    return decorator
