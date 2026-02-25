# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

#!/usr/bin/env python3
"""Minimal smoke test for the gateway health endpoint.

Usage:
  python3 api/src/gateway/smoke_test.py --url http://127.0.0.2:18789/health
"""

from __future__ import annotations

import argparse
import sys

import httpx


def main() -> int:
    parser = argparse.ArgumentParser(description="Gateway smoke test")
    parser.add_argument(
        "--url",
        default="http://127.0.0.2:18789/health",
        help="Health endpoint URL",
    )
    args = parser.parse_args()

    try:
        resp = httpx.get(args.url, timeout=2.0)
    except Exception as exc:
        print(f"Gateway not responding: {exc}")
        return 1

    if resp.status_code != 200:
        print(f"Gateway health check failed: {resp.status_code}")
        print(resp.text)
        return 1

    print(resp.text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
