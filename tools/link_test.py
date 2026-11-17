#!/usr/bin/env python3
"""link_test.py: the token store, its code cache, the link and LIST stay right across edits and RUN."""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import dosrun

# A subroutine bigger than the code cache: it empties mid-run, with the
# FOR, GOSUB and FN frames live.
def flush(n: int) -> str:
    return (
        "10 DEF FNA(X)=X+1\n20 FOR I=1 TO 3\n30 GOSUB 1000\n40 NEXT\n50 PRINT Z;Y;T:END\n"
        "1000 FOR J=1 TO 2:T=T+FNA(J):GOSUB 2000:NEXT:RETURN\n"
        + "".join(f"{2000 + k} Z=Z+1:Y=Y+1:IF Z<0 THEN PRINT Z\n" for k in range(n))
        + f"{2000 + n} RETURN\nRUN"
    )


FLUSH = flush(500)

CHURN = "\n".join(f'10 PRINT "{i:<60}":X=1+2+3+4' for i in range(1500))

CASES = {
    # Ops written out as machine code: each relation, INT and FLOAT, as a
    # value and as an IF.
    "10 A%=3:B%=5:C=3:D=5:E=-2.5\n"
    "20 PRINT (A%<B%);(A%>B%);(A%=B%);(A%<>B%);(A%<=B%);(A%>=B%);(A%=3);(A%>2)\n"
    "30 PRINT (C<D);(C>D);(C=D);(C<>D);(C<=D);(C>=D);(C=3);(C>2.5);(E<-2);(E>-3)\n"
    '40 IF A%<B% THEN PRINT "L1"\n50 IF A%>=B% THEN PRINT "BAD1"\n60 IF C<=D THEN PRINT "L2"\n'
    '70 IF C<>3 THEN PRINT "BAD2"\n80 IF A%<>B% AND C=3 THEN PRINT "L3"\n'
    "90 PRINT -A%;NOT A%;-C;-E;A%*B%;A%/2;A%-B%;C*D;C/D;5-C;A%+1;INT(E)\nRUN": [
        "-1  0  0 -1 -1  0 -1 -1", "-1  0  0 -1 -1  0 -1 -1 -1 -1", "L1", "L2", "L3",
        "-3 -4 -3  2.5  15  1 -2  15  .6  2  4 -3"],
    FLUSH: ["3000  3000  15"],
    # A stream past the cache's end (XSLK too small): a crash at a flush.
    flush(300): ["1800  1800  15"],
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
    # LIST prints the body: each spelling as typed, no extra spaces.
    '10 IF A>.5 THEN 20\n20 COUNT=1:PRINT CO;1.5E3;40000\n30 DEF FNSQR(X)=X*X\nLIST': [
        "10 IF A>.5 THEN 20", "20 COUNT=1:PRINT CO;1.5E3;40000", "30 DEF FNSQR(X)=X*X"],
    '10 PRINT "A";:PRINT TAB(5);B,:NEXT:NEXT I\nLIST': ['10 PRINT "A";:PRINT TAB(5);B,:NEXT:NEXT I'],
    # Without ST, LIST joined same-kind statements: READ A:READ B as READ A,B.
    "10 READ A:READ B:GET C:GET D:DIM E(1):DIM F(1):NEXT I:NEXT J\nLIST": [
        "10 READ A:READ B:GET C:GET D:DIM E(1):DIM F(1):NEXT I:NEXT J"],
    '10 PRINT#1,A;:PRINT B:CMD 1:PRINT C\nLIST': ['10 PRINT#1,A;:PRINT B:CMD 1:PRINT C'],
    "10 A=(B+C)*-D:E=2^-1:F=(G-H)-(I-J):K=NOT L\nLIST": ["10 A=(B+C)*-D:E=2^-1:F=(G-H)-(I-J):K=NOT L"],
    # READ scans the tokens for DATA: not after THEN, a ':' in quotes stays.
    "10 REM X:DATA 9\n20 IF 1 THEN DATA 8\n"
    '30 DATA 1:DATA "A:B":DATA 3\n40 READ A,B$,C:PRINT A;B$;C\nRUN': ["1 A:B 3"],
    "10 DATA 1,2:DATA 3\n20 DATA 4\n30 FOR I=1 TO 4:READ A:PRINT A;:NEXT:PRINT\n"
    "40 RESTORE 20:READ B:PRINT B\n50 READ C\nRUN": ["1  2  3  4", "4", "?OUT OF DATA ERROR IN 50"],
    # A jump's line is found once and patched into the cached code.
    '10 FOR I=1 TO 3:PRINT I;:ON I GOTO 20,20,30\n20 NEXT\n30 PRINT "X":GOSUB 40:GOSUB 40:END\n'
    '40 PRINT "S":RETURN\nRUN': ["1  2  3 X", "S"],
    # GOTO, GOSUB find their line at the stream: back, forward, missing.
    "10 I=I+1:IF I<3 THEN 10\n20 GOSUB 60:GOTO 50\n40 PRINT 4\n50 PRINT I:GOTO 70\n"
    "60 PRINT 6:RETURN\n70 GOTO 90\nRUN": ["6", "3", "?UNDEF'D STATEMENT ERROR IN 70"],
    # NEXT of an INT variable is machine code: it falls through to the next op,
    # and an inner frame on top goes the slow way.
    "10 FOR K%=1 TO 2:FOR L%=1 TO 2:PRINT K%;L%;:NEXT L%,K%:PRINT\n"
    "20 FOR I%=1 TO 2:FOR J%=1 TO 2:NEXT I%:PRINT I%;J%\nRUN": [
        "1  1  1  2  2  1  2  2", "3  1"],
    # 1-D element loads and stores are machine code: INT and FLOAT, an
    # undimensioned array (the slow way), a subscript out of range.
    "10 DIM A%(5),B(5):FOR I=0 TO 5:A%(I)=I*2:B(I)=I*1.5:NEXT\n"
    "20 PRINT A%(3);A%(5);B(3);B(5);C%(7);D(2)\n30 PRINT A%(6)\nRUN": [
        "6  10  4.5  7.5  0  0", "?BAD SUBSCRIPT ERROR IN 30"],
    # LIST gives the line as typed, spaces and all.
    "10 A = B  +  1 : PRINT A,  C\nLIST": ["10 A = B  +  1 : PRINT A,  C"],
}


# A line rebuilt at the link left the end marker behind: ?OUT OF MEMORY at RUN.
CHECKERS = Path(__file__).parent.parent / "tests/corpus/bcg/23_checkers.bas"


def run_mode(fpu: str) -> None:
    os.environ["DOS_FPU"] = fpu
    jobs = {f"t{i}": f"{src}\n" for i, src in enumerate(CASES)}
    jobs["checkers"] = "\n".join(x for x in CHECKERS.read_text().splitlines() if x.strip() and len(x) < 80) + "\nRUN\n"
    r = dosrun.run(jobs, Path(tempfile.mkdtemp()) / "w")
    assert "OUT OF MEMORY" not in r["checkers"].out, f"DOS_FPU={fpu} checkers: {r['checkers'].out[-200:]!r}"
    for i, (src, want) in enumerate(CASES.items()):
        got = [x.strip("\r ") for x in r[f"t{i}"].out.split("\n")]
        for w in want:
            assert w in got, f"DOS_FPU={fpu} {src[:60]}: {r[f't{i}'].out[-300:]!r}"
        if src.endswith("LIST"):
            listed = [x for x in got if x[:1].isdigit() and " " in x]
            assert listed == [w for w in want if " " in w], f"DOS_FPU={fpu} {src[:60]}: {listed!r}"


if __name__ == "__main__":
    run_mode("0")
    run_mode("1")
    print("ok")
