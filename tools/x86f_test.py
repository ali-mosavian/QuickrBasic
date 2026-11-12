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
    # M1, control flow: block IF, one-line IF ... ELSE, DO, WHILE, SELECT CASE, EXIT, SWAP.
    "qb_blockif": ("FOR i=1 TO 3\r\nIF i=1 THEN\r\nPRINT \"a\";\r\nELSEIF i=2 THEN\r\nPRINT \"b\";\r\n"
                   "ELSE\r\nPRINT \"c\";\r\nEND IF\r\nNEXT\r\n", "abc", "ERROR"),
    # PRINT read ELSE as an expression: the THEN part did not compile, the false jump had no target.
    "qb_ifelse": ('IF 0 THEN PRINT "a" ELSE PRINT "b"\r\nIF 1 THEN PRINT "c" ELSE PRINT "d"\r\n', "b\r\nc\r\n", "ERROR"),
    "qb_loops": ("n=0: DO WHILE n<3: n=n+1: LOOP: PRINT n;\r\nDO: n=n+1: IF n=7 THEN EXIT DO\r\nLOOP UNTIL n>99: PRINT n;\r\n"
                 "WHILE n>0: n=n-2: WEND: PRINT n;\r\nFOR k=1 TO 9: IF k=4 THEN EXIT FOR\r\nNEXT: PRINT k\r\n",
                 " 3  7 -1  4 \r\n", "ERROR"),
    # T_INT is 0: an INT SELECT read as not compiled, ?SYNTAX ERROR.
    "qb_select": ("c%=2: SELECT CASE c%\r\nCASE 1: PRINT \"one\"\r\nCASE 2, 3: PRINT \"two\"\r\nCASE ELSE: PRINT \"x\"\r\n"
                  "END SELECT\r\nFOR v=1 TO 5: SELECT CASE v: CASE IS < 2: PRINT \"l\";: CASE 3 TO 4: PRINT \"m\";\r\n"
                  "CASE ELSE: PRINT \"h\";\r\nEND SELECT: NEXT\r\n", "two\r\nlhmmh", "ERROR"),
    # C64's FOR ran its body once when the start was past the limit.
    "qb_forskip": ('FOR i=5 TO 1: PRINT "no": NEXT: FOR j=1 TO 0 STEP -1: PRINT j;: NEXT: PRINT "y"\r\n',
                   " 1  0 y\r\n", "no"),
    # DEF FN twice: the name moved over FN's first byte when one space parted them; the load hung.
    "qb_fnspace": ("DEF FN twice(v) = v * 2\r\nPRINT FN twice(4)\r\n", " 8 \r\n", "ERROR"),
    "qb_swap": ("a=1: c=2: DIM z(3): z(1)=5: z(2)=9: SWAP a, c: SWAP z(1), z(2): PRINT a; c; z(1); z(2)\r\n",
                " 2  1  9  5 \r\n", "ERROR"),
    # a$ = b$ took b$'s heap block: once a$ changed, a GC dropped it and b$ printed "gh".
    "str_letvar": ('b$ = "x" + "y": a$ = b$: a$ = "q"\r\n'
                   'x$ = "abcdefghij": FOR k = 1 TO 9: x$ = x$ + x$: NEXT\r\n'
                   'FOR i = 1 TO 1000: c$ = x$ + STR$(i): NEXT\r\nPRINT b$\r\n',
                   "xy\r\n", "ERROR"),
    "qb_procs": ("x = 5: CALL hi(3): hi 4: PRINT sq(x) + 1\r\ny = 10: bump y: PRINT y: bump (y): PRINT y\r\n"
                 "END\r\nSUB hi (n)\r\nPRINT \"hi\"; n\r\nEND SUB\r\nFUNCTION sq (x)\r\nsq = x * x\r\nEND FUNCTION\r\n"
                 "SUB bump (v)\r\nv = v + 1\r\nEND SUB\r\n",
                 "hi 3 \r\nhi 4 \r\n 26 \r\n 11 \r\n 11 \r\n", "ERROR"),
    # PCALL bound a string argument the wrong way round: a$ was empty in the procedure.
    "qb_strparam": ('PRINT nm$("bob") + "!"\r\nFUNCTION nm$ (a$)\r\nnm$ = "<" + a$ + ">"\r\nEND FUNCTION\r\n',
                    "<bob>!\r\n", "ERROR"),
    # A parameter's AS type was not entered: the body's n was another variable, a SINGLE.
    "qb_paramas": ("c% = 4: inc c%: PRINT c%\r\nSUB inc (n AS INTEGER)\r\nn = n + 1\r\nEND SUB\r\n", " 5 \r\n", "ERROR"),
    # STATIC n in a FUNCTION took the module's DIM SHARED n: the sequence printed 100 101.
    "qb_static": ("DIM SHARED n AS INTEGER: n = 99: PRINT f; f; n\r\nFUNCTION f\r\nSTATIC n AS INTEGER\r\n"
                  "n = n + 1: f = n\r\nEND FUNCTION\r\n", " 1  2  99 \r\n", "ERROR"),
    # The control stack kept the C64's 191 bytes: fib(12) in a FOR was OUT OF MEMORY.
    "qb_recurse": ("DEFINT A-Z\r\nFOR i = 12 TO 12: PRINT fib(i): NEXT\r\nFUNCTION fib (n)\r\n"
                   "IF n < 2 THEN fib = n ELSE fib = fib(n - 1) + fib(n - 2)\r\nEND FUNCTION\r\n", " 144 \r\n", "ERROR"),
    # DIM a(0 TO 0) was a SYNTAX ERROR; a STATIC FUNCTION's DIM ran again: REDIM'D ARRAY.
    "qb_dimto": ("PRINT f; f\r\nFUNCTION f STATIC\r\nDIM c(0 TO 0)\r\nc(0) = c(0) + 1: f = c(0)\r\nEND FUNCTION\r\n",
                 " 1  2 \r\n", "ERROR"),
    "qb_exitsub": ("k = 7: s: s: PRINT k\r\nSUB s\r\nSHARED k\r\nFOR i = 1 TO 10\r\nIF i = 3 THEN EXIT SUB\r\n"
                   "PRINT i;\r\nk = k + 1\r\nNEXT\r\nEND SUB\r\n", " 1  2  1  2  11 \r\n", "ERROR"),
    "qb_intops": ("PRINT 17 \\ 3; -17 \\ 3; 17 MOD 5; -17 MOD 5; 5 XOR 3; 2 + 7 MOD 4 * 1; 6 EQV 3; 0 IMP 0\r\n",
                  " 5 -5  2 -2  6  5 -6 -1 \r\n", "ERROR"),
    # \\ MOD XOR's operator codes, 10 and up, took the relations (80h + mask) too: p <> 13 was a
    # BAD SUBSCRIPT.
    "qb_relop": ("p = 12.6: PRINT p <> 13; p < 13; p = 12.6\r\n", "-1 -1 -1 \r\n", "ERROR"),
    "qb_conv": ("PRINT CINT(2.5); CINT(3.5); CLNG(40000.4); FIX(-2.7); INT(-2.7); CSNG(3); CDBL(1.5)\r\n",
                " 2  4  40000 -2 -3  3  1.5 \r\n", "ERROR"),
    # INSTR's count was lost under str_get's CX: INSTR(4, "hello", "l") searched from 1, gave 3.
    "qb_instr": ('PRINT INSTR("hello", "l"); INSTR(4, "hello", "l"); INSTR(8, "abc", "a"); INSTR("abc", "")\r\n',
                 " 3  4  0  1 \r\n", "ERROR"),
    "qb_strfn": ('PRINT "["; SPACE$(2); "]"; STRING$(3, "x"); STRING$(2, 65); "["; LTRIM$("  a "); "]["; '
                 'RTRIM$(" b  "); "]"; UCASE$("aBc"); LCASE$("AbC"); HEX$(255); OCT$(8); HEX$(-1)\r\n',
                 "[  ]xxxAA[a ][ b]ABCabcFF10FFFFFFFF\r\n", "ERROR"),
    "qb_bounds": ("OPTION BASE 1\r\nDIM v(-2 TO 2), w(2, 3)\r\nFOR i = -2 TO 2: v(i) = i * 11: NEXT\r\n"
                  "w(2, 3) = 7: PRINT v(-2); v(2); LBOUND(v); UBOUND(v); LBOUND(w, 2); UBOUND(w, 2); w(2, 3)\r\n",
                  "-22  22 -2  2  1  3  7 \r\n", "ERROR"),
    "qb_fixstr": ('DIM f AS STRING * 5: PRINT "["; f; "]"; LEN(f)\r\nf = "cat": PRINT "["; f; "]": f = "abcdefg": PRINT f\r\n'
                  't$ = "abcdef": MID$(t$, 2, 3) = "XYZW": PRINT t$\r\n',
                  "[     ] 5 \r\n[cat  ]\r\nabcde\r\naXYZef\r\n", "ERROR"),
    # A record is one block, as QBasic lays it out: SHORT 2 bytes, INTEGER 4, STRING * n n.
    "qb_recmem": ("TYPE V\r\nx AS SHORT\r\ny AS INTEGER\r\nt AS STRING * 3\r\nEND TYPE\r\n"
                  'DIM a AS V, b AS V: a.x = &H1234: a.y = -1: a.t = "ab": b = a: a.x = 0\r\n'
                  "DEF SEG = VARSEG(b): PRINT LEN(b); LEN(b.t); PEEK(VARPTR(b)); PEEK(VARPTR(b) + 1); "
                  "PEEK(VARPTR(b.t) + 2): DEF SEG\r\nPRINT b.x; b.y; b.t; \"|\"\r\n",
                  " 9  3  52  18  32 \r\n 4660 -1 ab |\r\n", "ERROR"),
    "qb_short": ("TYPE V\r\nx AS SHORT\r\nEND TYPE\r\nDIM a AS V: a.x = -32768: PRINT a.x: a.x = 32768\r\n",
                 "-32768 \r\n?OVERFLOW", None),
    # DIM v AS a TYPE skipped two bytes of the TYPE's name: SYNTAX ERROR. rc_ref lost a(i)'s
    # '(' to var_find's EDX: SYNTAX ERROR.
    "qb_type": ("TYPE P\r\nx AS LONG\r\nt AS STRING * 2\r\nEND TYPE\r\nTYPE L\r\na AS P\r\nb AS P\r\nEND TYPE\r\n"
                'DIM c AS L, d AS L, q(3) AS P\r\nc.a.x = 5: c.b.t = "xyz": d = c: c.a.x = 0\r\n'
                "q(2).x = 9: q(1).x = q(2).x + 1\r\nPRINT d.a.x; d.b.t; q(1).x; q(2).x\r\n",
                " 5 xy 10  9 \r\n", "ERROR"),
    "int32_lit": ("A%=65535:PRINT A%*2\r\n", " 131070 \r\n", "ERROR"),
}

# Typed at the prompt, not run as a file.
REPL = {
    # A direct line ran from DCODE, its statement map's place: nothing printed.
    "repl_direct": ("PRINT 5\r\n", " 5 \r\n", "ERROR"),
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
