#!/usr/bin/env python3
"""rustline_test.py: RUSTLINE's PLAY strings stay in the octaves PLAY takes; its effects stay on the synth.

"O7" in the spire's shot stopped the game with "Illegal function call" the first time a spire in
sight fired; the same in the battle tune's hats stopped it on the first bar.
"""

import re
from pathlib import Path

BAS = Path(__file__).resolve().parent.parent / "src/plat/wasm/web/demos/RUSTLINE.BAS"


def play_strings(src: str) -> list[str]:
    tunes = src.split("\nTunes:\n")[1].split("\nREM $STATIC")[0]
    lines = [x for x in src.splitlines() if re.search(r"\bPLAY\b", x)] + tunes.splitlines()
    return [s for x in lines for s in re.findall(r'"([^"]*)"', x)]


def test_octaves() -> None:
    bad = [s for s in play_strings(BAS.read_text()) if re.search(r"O[7-9]", s)]
    assert not bad, bad


def test_effects_on_synth() -> None:
    """The harvester's unload beep, a speaker SOUND, silenced the battle tune for 2.5 s an unload."""
    sfx = re.search(r"^SUB Sfx .*?^END SUB", BAS.read_text(), re.M | re.S).group(0)
    assert "SOUND" not in sfx, "a speaker SOUND in Sfx"
    bad = [s for x in sfx.splitlines() if "PLAY " in x for s in re.findall(r'"([^"]*)"', x) if s and "W" not in s]
    assert not bad, bad


if __name__ == "__main__":
    test_octaves()
    test_effects_on_synth()
    print("ok")
