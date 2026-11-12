#!/usr/bin/env python3
"""x86f_test.py [EXE]: regressions of the flat build (build/x86/basic32.exe).

Each case: its program, run as a file, the output it must print and one it must not."""

import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault("X86_COM", sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build/x86/basic32.exe"))
import x86run  # noqa: E402

CASES = {
    # ctail's inc esi after the and: ZF was the inc's, so a false IF ran the
    # rest of the line.
    "cmpif_false": ('A=5:IF A=4 THEN PRINT "NO":PRINT "NO"\r\nPRINT "END"\r\n', "END\r\n", "NO"),
    # cm_p's stack check kept the 16-bit room while each level took 12 bytes,
    # not 6: 14 parentheses were OUT OF MEMORY.
    "paren_depth": ("PRINT " + "(" * 29 + "7" + ")" * 29 + "\r\n", " 7 \r\n", "MEMORY"),
    # The lexer took a name after an A5h byte (spaces skipped) for FN's: 42405
    # is M_INT A5h A5h, so A was FN A.
    "fn_lookback": ("PRINT 42405; A\r\n", " 42405  0 \r\n", "ERROR"),
    # INT was 16-bit: these overflowed at 32767.
    "int32_add": ("A%=32767:A%=A%+1:PRINT A%\r\n", " 32768 \r\n", "ERROR"),
    "int32_mul": ("A%=-70000:PRINT 300*300;A%*30000\r\n", " 90000 -2100000000 \r\n", "ERROR"),
    "int32_arr": ("DIM B%(3):B%(1)=70000:B%(2)=-5:PRINT B%(1);B%(2)\r\n", " 70000 -5 \r\n", "ERROR"),
    "int32_for": ("FOR I%=32766 TO 32768:PRINT I%;:NEXT\r\n", " 32766  32767  32768 ", "ERROR"),
    "int32_ovf": ("A%=65536:PRINT A%*32768\r\n", "?OVERFLOW", None),
    "int32_toi": ("A=2^30:A%=A*2-128:PRINT A%:A%=-A*2:PRINT A%:A%=A*2\r\n",
                  " 2147483520 \r\n-2147483648 \r\n?ILLEGAL QUANTITY", None),
    # INT functions left the upper 16 bits unset: ASC("B")*1000 was 1507394000.
    "int32_fns": ('PRINT ASC("B")*1000;LEN("ABC")*100000;SGN(-5)\r\n', " 66000  300000 -1 \r\n", "ERROR"),
    # -literal folded into an unsigned I.LD.I16: -5 was 65531.
    "int32_neg": ("A%=-5:PRINT A%;-40000\r\n", "-5 -40000 \r\n", "ERROR"),
    # Names were resolved by the lexer, not the variable manager: a and a(),
    # b and b! were one symbol, b% and b& another.
    "qb_names": ('a=5:a(3)=9:b%=1:b&=2:b!=3:b=4:x.y.z=6\r\nPRINT a;a(3);b%;b&;b!;x.y.z\r\n',
                 " 5  9  1  2  4  6 \r\n", "ERROR"),
    # DEF FN's name came from an FN token: FNsq is one name now.
    "qb_deffn": ("DEF FNsq(x)=x*x\r\nPRINT FNsq(7)\r\n", " 49 \r\n", "ERROR"),
    # A plain FOR variable is SINGLE, as in QBasic; C64's inference made it INT.
    "qb_for_single": ("FOR i=46341 TO 46341:PRINT i*i:NEXT\r\n", " 2.147488E+09 \r\n", "ERROR"),
    # C64's 10-column zones and 0-based TAB: QBasic's are 14 and from 1, a
    # TAB behind the column on the next line.
    "qb_zone": ("PRINT 1,2\r\n", " 1 " + " " * 11 + " 2 \r\n", "ERROR"),
    "qb_tab": ('PRINT TAB(5);"x";TAB(3);"y"\r\n', "    x\r\n  y\r\n", "ERROR"),
    # INPUT "p", x printed "p? ": a comma means no ?. Redirected input is not echoed.
    "qb_input": (('INPUT "a", x\r\nINPUT "b"; y\r\nINPUT z\r\nPRINT x; y; z\r\n', "1\r\n2\r\n3\r\n"),
                 "a\r\nb? \r\n? \r\n 1  2  3 \r\n", "ERROR"),
    # RESTORE took a line number only: a label was ?UNDEF'D STATEMENT.
    "qb_restore": ("READ a\r\nRESTORE two\r\nREAD b\r\nPRINT a; b\r\nDATA 1\r\ntwo:\r\nDATA 2\r\n",
                   " 1  2 \r\n", "ERROR"),
    # An error on a line with no number said IN 65535, NONUM.
    "qb_err_nonum": ("PRINT 1/0\r\n", "?DIVISION BY ZERO ERROR", "65535"),
    # INT / INT truncated, C64-BC style: QBasic's / is a FLOAT.
    "qb_div": ("A%=7:PRINT A%/2;7/2\r\n", " 3.5  3.5 \r\n", "ERROR"),
    # DIM AS, DEFtype and CONST were syntax errors; their types are entered with the line, so a
    # line before the declaration sees them too.
    "qb_decl": ("GOTO 1\r\n2 PRINT a; s; v; x(2); PI; T$; N: END\r\n1 DEFINT A-Z\r\n"
                "DIM s AS STRING, v AS SINGLE: DIM x(5) AS LONG\r\nCONST N = 4, PI = 3.5, T$ = \"t\"\r\n"
                "a = 7 / 2: s = \"hi\": v = 7 / 2: x(2) = 70000: GOTO 2\r\n",
                " 4 hi 3.5  70000  3.5 t 4 \r\n", "ERROR"),
    # FLOAT to INT floored, as C64: QBasic rounds, a half to even.
    "qb_round": ('A%=2.5:B%=3.5:C%=-2.5:D%=-2.6:PRINT A%;B%;C%;D%;CHR$(65.6)\r\n', " 2  4 -2 -3 B\r\n", "ERROR"),
    "int32_lit": ("A%=65535:PRINT A%*2\r\n", " 131070 \r\n", "ERROR"),
}

# Typed at the prompt, not run as a file.
REPL = {
    # A numbered line stepped 3 bytes past its M_INT, which is 5: the line lost its first bytes.
    "repl_numbered": ("10 PRINT 5\r\n20 PRINT 6\r\nRUN\r\n", " 5 \r\n 6 \r\n", "ERROR"),
}


def main() -> None:
    tmp = Path(tempfile.mkdtemp())
    res = x86run.run({k: v[0] for k, v in CASES.items()}, tmp / "w", 10000, file=True)
    res |= x86run.run({k: v[0] for k, v in REPL.items()}, tmp / "r", 10000)
    every = CASES | REPL
    bad = [k for k, (_, want, forbid) in every.items() if want not in res[k].out or (forbid and forbid in res[k].out)]
    for k in bad:
        print(f"FAIL {k}: {res[k].reason} {res[k].out!r}")
    sys.exit(1 if bad else print("ok") or 0)


if __name__ == "__main__":
    main()
