#!/usr/bin/env python3
"""x86link_test.py: compiling on entry, the link and LIST from code stay right across edits and RUN."""

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
    # READ restarts at the first DATA, not after its first item.
    "10 DATA 1,2\n20 DATA 3\n30 READ A,B,C:RESTORE 20:READ D:PRINT A;B;C;D\nRUN": ["1  2  3  3"],
    # LIST decompiles: each spelling as typed, no extra spaces.
    '10 IF A>.5 THEN 20\n20 COUNT=1:PRINT CO;1.5E3;40000\n30 DEF FNSQR(X)=X*X\nLIST': [
        "10 IF A>.5 THEN 20", "20 COUNT=1:PRINT CO;1.5E3;40000", "30 DEF FNSQR(X)=X*X"],
    '10 PRINT "A";:PRINT TAB(5);B,:NEXT:NEXT I\nLIST': ['10 PRINT "A";:PRINT TAB(5);B,:NEXT:NEXT I'],
    # No statement op: LIST joined same-kind statements, READ A:READ B as READ A,B.
    "10 READ A:READ B:GET C:GET D:DIM E(1):DIM F(1):NEXT I:NEXT J\nLIST": [
        "10 READ A:READ B:GET C:GET D:DIM E(1):DIM F(1):NEXT I:NEXT J"],
    '10 PRINT#1,A;:PRINT B:CMD 1:PRINT C\nLIST': ['10 PRINT#1,A;:PRINT B:CMD 1:PRINT C'],
    "10 A=(B+C)*-D:E=2^-1:F=(G-H)-(I-J):K=NOT L\nLIST": ["10 A=(B+C)*-D:E=2^-1:F=G-H-(I-J):K=NOT L"],
}


# A line rebuilt at the link left the end marker behind: ?OUT OF MEMORY at RUN.
CHECKERS = Path(__file__).parent.parent / "test/corpus/bcg/23_checkers.bas"


def run_mode(fpu: str) -> None:
    os.environ["X86_FPU"] = fpu
    jobs = {f"t{i}": f"{src}\n" for i, src in enumerate(CASES)}
    jobs["checkers"] = "\n".join(x for x in CHECKERS.read_text().splitlines() if x.strip() and len(x) < 80) + "\nRUN\n"
    r = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w")
    assert "OUT OF MEMORY" not in r["checkers"].out, f"X86_FPU={fpu} checkers: {r['checkers'].out[-200:]!r}"
    for i, (src, want) in enumerate(CASES.items()):
        got = [x.strip("\r ") for x in r[f"t{i}"].out.split("\n")]
        for w in want:
            assert w in got, f"X86_FPU={fpu} {src[:60]}: {r[f't{i}'].out[-300:]!r}"
        if src.endswith("LIST"):
            listed = [x for x in got if x[:1].isdigit() and " " in x]
            assert listed == [w for w in want if " " in w], f"X86_FPU={fpu} {src[:60]}: {listed!r}"


if __name__ == "__main__":
    run_mode("0")
    run_mode("1")
    print("ok")
