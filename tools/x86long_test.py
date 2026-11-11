#!/usr/bin/env python3
"""x86long_test.py: strings past 255 chars and arrays past 64K, which stock's limits forbade."""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import x86run

ABC = "".join(chr(65 + i % 26) for i in range(1, 1001))

NEST = '"A"+(' * 9 + '"A"' + ')' * 9

CASES = {
    # A 1000-char string: LEN, MID$ and RIGHT$ past 255, PRINT of all of it.
    '10 S$="":FOR I=1 TO 1000:S$=S$+CHR$(65+I-INT(I/26)*26):NEXT\n'
    "20 PRINT LEN(S$);ASC(MID$(S$,999,1));RIGHT$(S$,3)\n30 PRINT S$\nRUN": [
        f" 1000  {ord(ABC[998])} {ABC[-3:]}",
        ABC,
    ],
    # Doubling past 32767 chars.
    '10 S$="X":FOR I=1 TO 15:PRINT LEN(S$):S$=S$+S$:NEXT\nRUN': [" 16384", "?STRING TOO LONG ERROR IN 10"],
    # 3000-char strings past the heap's size: GC moves blocks over 256 bytes.
    '10 S$="":FOR I=1 TO 300:S$=S$+"ABCDEFGHIJ":NEXT:DIM A$(20)\n'
    "20 FOR I=1 TO 400:K=I-INT(I/21)*21:A$(K)=STR$(I)+S$:NEXT\n"
    "30 E=0:FOR K=0 TO 20:IF A$(K)<>STR$(VAL(A$(K)))+S$ THEN E=E+1\n"
    "40 NEXT:PRINT E;LEN(A$(1))\nRUN": [" 0  3004"],
    # Ten temporaries wait in one expression; stock allowed three.
    "10 X$=" + NEST + "\n20 PRINT LEN(X$);X$\nRUN": [" 10 " + "A" * 10],
    # Temporaries wait while the heap fills and is collected.
    '10 S$="":FOR I=1 TO 300:S$=S$+"ABCDEFGHIJ":NEXT\n'
    '20 FOR I=1 TO 400:X$=S$+("A"+("B"+("C"+STR$(I)))):NEXT\n30 PRINT LEN(X$);RIGHT$(X$,6)\nRUN': ["3007 BC 400"],
    # Arrays past 64K: a number, an INT 2-D and a string array.
    "10 DIM A(30000),B%(2,20000),C$(20000):A(29999)=7:B%(2,20000)=5:B%(0,1)=3\n"
    '20 C$(20000)="Z":C$(1)="Y"\n30 PRINT A(29999);A(0);B%(2,20000);B%(0,1);C$(20000);C$(1)\nRUN': [
        " 7  0  5  3 ZY"
    ],
}


def flat(out: str) -> list[str]:
    """Lines, and the whole output unwrapped, for a PRINT past the screen's width."""
    lines = [x.strip("\r ") for x in out.split("\n")]
    return lines + ["".join(x.strip("\r") for x in out.split("\n"))]


def run_mode(fpu: str) -> None:
    os.environ["X86_FPU"] = fpu
    jobs = {f"t{i}": f"{src}\n" for i, src in enumerate(CASES)}
    r = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w")
    for i, (src, want) in enumerate(CASES.items()):
        got = flat(r[f"t{i}"].out)
        for w in want:
            assert any(w in g for g in got), f"X86_FPU={fpu} {src[:60]!r}: {r[f't{i}'].out[-300:]!r}"


if __name__ == "__main__":
    run_mode("0")
    run_mode("1")
    print("ok")
