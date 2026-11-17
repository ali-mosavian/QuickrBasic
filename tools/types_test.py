#!/usr/bin/env python3
"""types_test.py: static types differ from stock on purpose; stock's transcripts cannot check them."""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import dosrun

CASES = {
    "PRINT 1/3;7/2;-7/2;2^3;2^-1;(-1)^3;(-1)^-3;0^0": " 0  3 -3  8  0 -1 -1  1",
    "PRINT 300*100": " 30000",
    "PRINT 32767+1": "?OVERFLOW ERROR",
    "PRINT 300*300": "?OVERFLOW ERROR",
    "PRINT 2^16": "?OVERFLOW ERROR",
    "I%=-32768:PRINT I%/-1": "?OVERFLOW ERROR",
    "I%=-32768:PRINT -I%": "?OVERFLOW ERROR",
    "PRINT 5/0": "?DIVISION BY ZERO ERROR",
    "A=1:PRINT A/3*3;2.5/2;1/2.5;A^-1": " 1  1.25  .4  1",
    "I%=7:J%=2:A=2:PRINT I%/J%;I%/A;A/J%;I%+J%;I%-A": " 3  3.5  1  9  5",
    'PRINT 1+"A"': "?TYPE MISMATCH ERROR",
    "10 FOR I=1 TO 3:PRINT I/2;:NEXT:PRINT I\nRUN": "0  1  1  4",
    "10 FOR I=1 TO 3:I=I+.5:PRINT I/2;:NEXT\nRUN": ".75  1.5",
    "10 N=3:FOR I=1 TO N:PRINT I/2;:NEXT\nRUN": ".5  1  1.5",
    "10 FOR J=1 TO 2:FOR I=1 TO J:NEXT:PRINT I/2;:NEXT\nRUN": "1  1",
    "10 FOR I=1 TO 2:NEXT:PRINT X/2:X=1\nRUN": "0",
    "10 FOR I=1 TO 3:NEXT:READ I:PRINT I/2:DATA 3\nRUN": "1.5",
    "10 DEF FN F(I)=I/2:FOR I=1 TO 3:NEXT:PRINT FN F(3)\nRUN": "1.5",
    "10 FOR I=1 TO 3:NEXT\nI=5\nPRINT I\n20 I=2.5:PRINT I\nRUN": "2.5",
}


def run_mode(fpu: str) -> None:
    os.environ["DOS_FPU"] = fpu
    jobs = {f"t{i}": f"{src}\n" for i, src in enumerate(CASES)}
    r = dosrun.run(jobs, Path(tempfile.mkdtemp()) / "w")
    for i, (src, want) in enumerate(CASES.items()):
        got = [x.strip("\r ") for x in r[f"t{i}"].out.split("\n")]
        assert want.strip() in got, f"DOS_FPU={fpu} {src}: {r[f't{i}'].out!r}"


if __name__ == "__main__":
    run_mode("0")
    run_mode("1")
    print("ok")
