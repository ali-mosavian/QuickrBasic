#!/usr/bin/env python3
"""mothassets.py [--check]: MOTHLITE.BAS's moth sprites (Spr), drawn by tools/mothart.bas in
DOSBox and written into the demo as DATA between its MothArt markers: drawing them took 146M of
the start-up's 176M instructions. --check: fail when the demo's DATA is not what the art draws.

The DATA: a count n then n values; -n then one value n times; 0 the end."""

import re
import sys
import struct
import argparse
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/MOTHLITE.BAS"
ART = ROOT / "tools/mothart.bas"
sys.path.insert(0, str(ROOT / "tools"))
import dosrun  # noqa: E402

BEGIN, END = "' MothArt's begin.\n", "' MothArt's end.\n"
DRIVER = """SCREEN 13
LoadTables
BuildMoth
OPEN "SPR.BIN" FOR BINARY AS #1
DIM i AS INTEGER
FOR i = 0 TO UBOUND(Spr): PUT #1, , Spr(i): NEXT
CLOSE #1
END
"""
WIDTH = 78  # a DATA line's most characters


def art_program(demo: str) -> str:
    """The demo with the art's types and declarations first, its routines last, DRIVER its module code."""
    art = ART.read_text()
    first = re.search(r"^(?:'.*\n)*(?:SUB|FUNCTION) ", art, re.M)
    assert first, "mothart.bas has no routine"
    at = first.start()
    head, rest = demo.split("\nMain\nEND\n", 1)
    return art[:at] + head + "\n" + DRIVER + rest + "\n" + art[at:]


def drawn() -> list[int]:
    """Spr as the art draws it, in DOSBox: the host build's screen is not the DOS one's."""
    gfx = {"lib/gfx.qbx": (ROOT / "build/c/lib/gfx.qbx").read_bytes()}
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp) / "w"
        res = dosrun.run(
            {"art": (art_program(DEMO.read_text()), "")},
            work,
            limit_ms=600000,
            file=True,
            files=gfx,
            mem=16,
            screen=True,
        )["art"]
        data = (work / "SPR.BIN").read_bytes() if (work / "SPR.BIN").exists() else b""
        if not data:
            raise SystemExit(f"mothassets: no sprites drawn: {res.reason} {res.out[-600:]}")
        return list(struct.unpack(f"<{len(data) // 4}i", data))


def encode(vals: list[int]) -> list[int]:
    """The run-length stream: runs of 3 or more alike, else literals."""
    out: list[int] = []
    lit: list[int] = []
    i = 0
    while i < len(vals):
        j = i
        while j < len(vals) and vals[j] == vals[i]:
            j += 1
        if j - i >= 3:
            if lit:
                out += [len(lit), *lit]
                lit = []
            out += [-(j - i), vals[i]]
        else:
            lit += vals[i:j]
        i = j
    if lit:
        out += [len(lit), *lit]
    return [*out, 0]


def decode(stream: list[int]) -> list[int]:
    """encode's values back, as LoadMoth reads them."""
    out: list[int] = []
    i = 0
    while stream[i]:
        n = stream[i]
        if n > 0:
            out += stream[i + 1 : i + 1 + n]
            i += 1 + n
        else:
            out += [stream[i + 1]] * -n
            i += 2
    return out


def block(vals: list[int]) -> str:
    """The DATA between the markers."""
    lines, cur = ["' Spr, as tools/mothart.bas draws it: written by tools/mothassets.py.", "MothArt:"], ""
    for v in encode(vals):
        item = str(v)
        if cur and len(cur) + 2 + len(item) > WIDTH:
            lines.append(cur)
            cur = ""
        cur = f"{cur}, {item}" if cur else f"DATA {item}"
    return BEGIN + "\n".join([*lines, cur]) + "\n" + END


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true", help="fail when the DATA is stale")
    a = ap.parse_args()
    vals = drawn()
    assert decode(encode(vals)) == vals
    src = DEMO.read_text()
    pre, rest = src.split(BEGIN, 1)
    new = pre + block(vals) + rest.split(END, 1)[1]
    if a.check:
        sys.exit(0 if new == src else "mothassets: MOTHLITE.BAS's MothArt is stale; run tools/mothassets.py")
    DEMO.write_text(new)
    print(f"mothassets: {len(vals)} values, {len(block(vals))} characters")


if __name__ == "__main__":
    main()
