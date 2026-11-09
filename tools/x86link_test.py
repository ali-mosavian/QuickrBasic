#!/usr/bin/env python3
"""x86link_test.py: code compiled on entry stays right across edits, RUN and the link."""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import x86run

CHURN = "\n".join(f'10 PRINT "{i:<60}":X=1+2+3+4' for i in range(1500))

CASES = {
    "10 GOTO 30\n20 PRINT 2:END\n30 PRINT 3\nRUN\n25 REM\n30 PRINT 4\nRUN": ["3", "4"],
    "10 GOTO 30\n30 PRINT 3\nRUN\n30\nRUN": ["3", "?UNDEF'D STATEMENT ERROR IN 10"],
    "10 PRINT 7\nGOTO 10": ["7"],
    "10 ON 2 GOTO 20,30\n20 PRINT 2:END\n30 PRINT 3\nRUN": ["3"],
    "10 GOSUB 30:PRINT 1:END\n30 PRINT 2:RETURN\nRUN": ["2", "1"],
    "10 FOR I=1 TO 2\n20 PRINT I\n30 NEXT\nRUN\n15 REM\nRUN": ["1", "2"],
    "10 PRINT 1:STOP:PRINT 2\nRUN\nCONT": ["1", "2"],
    "10 FOR I=1 TO 3:NEXT:PRINT I/3\nRUN\n20 I=1.5\nRUN": ["1", "1.333333"],
    f"{CHURN}\n20 PRINT 9\nRUN": ["9"],
}


def run_mode(fpu: str) -> None:
    os.environ["X86_FPU"] = fpu
    jobs = {f"t{i}": f"{src}\n" for i, src in enumerate(CASES)}
    r = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w")
    for i, (src, want) in enumerate(CASES.items()):
        got = [x.strip("\r ") for x in r[f"t{i}"].out.split("\n")]
        for w in want:
            assert w in got, f"X86_FPU={fpu} {src[:60]}: {r[f't{i}'].out[-300:]!r}"


if __name__ == "__main__":
    run_mode("0")
    run_mode("1")
    print("ok")
