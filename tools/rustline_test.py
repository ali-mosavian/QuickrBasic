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


def battle_rows(src: str) -> list[list[str]]:
    """The battle tune's bars: the Tunes rows of four strings (bass, lead, stabs, drums)."""
    tunes = src.split("\nTunes:\n")[1].split("\nREM $STATIC")[0]
    rows = [re.findall(r'"([^"]*)"', x) for x in tunes.splitlines()]
    return [r for r in rows if len(r) == 4]


def test_octaves() -> None:
    bad = [s for s in play_strings(BAS.read_text()) if re.search(r"O[7-9]", s)]
    assert not bad, bad


def test_effects_on_synth() -> None:
    """The harvester's unload beep, a speaker SOUND, silenced the battle tune for 2.5 s an unload."""
    sfx = re.search(r"^SUB Sfx .*?^END SUB", BAS.read_text(), re.M | re.S).group(0)
    assert "SOUND" not in sfx, "a speaker SOUND in Sfx"
    bad = [s for x in sfx.splitlines() if "PLAY " in x for s in re.findall(r'"([^"]*)"', x) if s and "W" not in s]
    assert not bad, bad


def test_battle_bass_is_low() -> None:
    """The battle tune's bass ran at 220 to 440 Hz, under 1% of the mix below 120 Hz: it sounded thin."""
    rows = battle_rows(BAS.read_text())
    bad = [r[0] for r in rows if len(r) == 4 and "O" in r[0] and not re.search(r"O[01]", r[0])]
    assert not bad, bad[:2]


def whole_notes(tune: str) -> float:
    """A PLAY string's length in whole notes (every note and rest carries its own length)."""
    return sum(1.5 ** bool(d) / int(n) for n, d in re.findall(r"[A-GP][#+-]?(\d+)(\.?)", re.sub(r"O\d", "", tune)))


def test_battle_bars_are_whole() -> None:
    """A voice whose bar is not a whole note drifts off the others, a bar is queued every 4 beats."""
    rows = battle_rows(BAS.read_text())
    bars = [r for r in rows if r[0] and not r[0].startswith("T")]
    bad = [t for r in bars for t in r[:3] if abs(whole_notes(t) - 1) > 1e-9]
    assert not bad, bad[:2]


if __name__ == "__main__":
    test_octaves()
    test_effects_on_synth()
    test_battle_bass_is_low()
    test_battle_bars_are_whole()
    print("ok")
