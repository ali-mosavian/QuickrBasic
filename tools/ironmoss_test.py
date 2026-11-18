#!/usr/bin/env python3
"""ironmoss_test.py: IRONMOSS's PLAY strings stay in the octaves PLAY takes.

"O7" in the spire's shot stopped the game with "Illegal function call" the first time a spire in
sight fired; the same in the battle tune's hats stopped it on the first bar.
"""

import re
from pathlib import Path

BAS = Path(__file__).resolve().parent.parent / "src/plat/wasm/web/demos/IRONMOSS.BAS"


def play_strings(src: str) -> list[str]:
    tunes = src.split("\nTunes:\n")[1].split("\nREM $STATIC")[0]
    lines = [x for x in src.splitlines() if re.search(r"\bPLAY\b", x)] + tunes.splitlines()
    return [s for x in lines for s in re.findall(r'"([^"]*)"', x)]


def test_octaves() -> None:
    bad = [s for s in play_strings(BAS.read_text()) if re.search(r"O[7-9]", s)]
    assert not bad, bad


if __name__ == "__main__":
    test_octaves()
    print("ok")
