"""Minimal Python fixture used by libs/parse tests.

Exercises module-level imports, dataclasses, type annotations,
generators, async functions, context managers, and f-strings — the
grammar paths most production Python code we'll scan goes through.
The triple-quoted docstring is the indentation-tracking external
scanner's first real test, since tree-sitter-python's scanner.c is
responsible for the INDENT/DEDENT tokens the grammar relies on.
"""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import Generator


@dataclass
class Greeting:
    subject: str
    reps: int = 1

    def render(self) -> str:
        return f"Hello, {self.subject}!"


def add(a: int, b: int) -> int:
    return a + b


def counted(g: Greeting) -> Generator[str, None, None]:
    for _ in range(g.reps):
        yield g.render()


async def main() -> int:
    g = Greeting(subject="world", reps=add(40, 2))
    for line in counted(g):
        print(line)
    return 0


if __name__ == "__main__":
    asyncio.run(main())
