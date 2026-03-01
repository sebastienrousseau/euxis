#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Shared performance utilities for parsing runtime metric streams."""

from __future__ import annotations

import json
import math
from pathlib import Path


def parse_json_stream(text: str) -> list[dict]:
    decoder = json.JSONDecoder()
    rows: list[dict] = []
    index = 0
    size = len(text)
    while index < size:
        while index < size and text[index].isspace():
            index += 1
        if index >= size:
            break
        obj, next_index = decoder.raw_decode(text, index)
        if isinstance(obj, dict):
            rows.append(obj)
        index = next_index
    return rows


def extract_latencies_ms(metrics_path: Path) -> list[float]:
    rows = parse_json_stream(metrics_path.read_text(encoding="utf-8"))
    latencies: list[float] = []
    for row in rows:
        value = row.get("latency_ms")
        if value is None:
            value = row.get("ms")
        if isinstance(value, (int, float)):
            latencies.append(float(value))
    return latencies


def p95(values: list[float]) -> float:
    if not values:
        return math.inf
    values = sorted(values)
    idx = int(round(0.95 * (len(values) - 1)))
    return values[idx]
