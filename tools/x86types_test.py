#!/usr/bin/env python3
"""x86types_test.py: static types differ from stock on purpose; stock's transcripts cannot check them."""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import x86run

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
}


def run_mode(fpu: str) -> None:
    os.environ["X86_FPU"] = fpu
    jobs = {f"t{i}": f"{src}\n" for i, src in enumerate(CASES)}
    r = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w")
    for i, (src, want) in enumerate(CASES.items()):
        got = [x.strip("\r ") for x in r[f"t{i}"].out.split("\n")]
        assert want.strip() in got, f"X86_FPU={fpu} {src}: {r[f't{i}'].out!r}"


if __name__ == "__main__":
    run_mode("0")
    run_mode("1")
    print("ok")
