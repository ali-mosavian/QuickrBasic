#!/usr/bin/env python3
"""sb_test.py: the synthesizer's voices on DOS through DOSBox-X's Sound Blaster (dev/dos/m32/sb.c).

A program reads the card's DMA channel (BLASTER's D1) to see whether it runs, and PEEKs the buffer it plays."""

import sys
import tempfile
from pathlib import Path

import dosrun

# the DMA channel's address, polled for 0.3 s: the span it covers, and the samples in it
PROBE = r"""t# = TIMER: DO: LOOP WHILE TIMER - t# < .5
lo& = 99999: hi& = 0: t# = TIMER
DO
OUT &HC, 0: a& = INP(2) + 256& * INP(2): pg = INP(&H83)
IF a& < lo& THEN lo& = a&
IF a& > hi& THEN hi& = a&
LOOP WHILE TIMER - t# < .3
vlo = 255: vhi = 0
FOR i = lo& TO hi&: v = PEEK(pg * 65536 + i)
IF v < vlo THEN vlo = v
IF v > vhi THEN vhi = v
NEXT
PRINT hi& - lo&; vlo; vhi
"""
VOICES = 'PLAY "MB T120 L1 W2 V15 O3 C C", "L1 W1 V15 O4 E E"\n' + PROBE
OLD = 'PLAY "MB T120 L1 O3 C C"\n' + PROBE
PLAIN = 'PLAY "T255 W2 V12 L16 C E", "W1 L16 E G"\nPRINT "ok"\n'


def numbers(text: str) -> list[int]:
    return [int(x) for x in text.split()]


def main() -> None:
    work = Path(tempfile.mkdtemp())
    r = dosrun.run({"voices": VOICES, "old": OLD}, work / "sb", limit_ms=20000, file=True)
    span, low, high = numbers(r["voices"].out)
    # a note for the synthesizer starts the card: its 1 KB buffer plays round, the voices' samples in it
    assert r["voices"].reason == "exit" and span > 900 and 60 <= low < high <= 200 and high - low > 40, r["voices"]
    # QB's PLAY never touches the card
    assert numbers(r["old"].out)[0] == 0, r["old"]
    # no BLASTER: the speaker plays the first voice, and the program ends
    r = dosrun.run({"plain": PLAIN}, work / "none", limit_ms=20000, file=True, before=("SET BLASTER=",))
    assert r["plain"].reason == "exit" and "ok" in r["plain"].out, r["plain"]
    print("ok")


if __name__ == "__main__":
    sys.exit(main())
