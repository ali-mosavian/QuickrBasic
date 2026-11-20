#!/usr/bin/env python3
"""regress_test.py [EXE]: regressions of the DOS build (build/c/basic32.exe).

Each case: its program, run as a file, the output it must print and one it must not."""

import re
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault("BASIC_EXE", sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build/c/basic32.exe"))
import dosrun  # noqa: E402

CASES = {
    # C port: TIMER was DOS's time, in 55 ms steps; it is llrm's (B$TIMR), the timer chip's count
    # within the tick. NIBBLES.BAS's waits of curSpeed / 2000 s took a whole tick each.
    "c_timer_fine": ("t = TIMER: DO: u = TIMER: LOOP WHILE u = t: PRINT \"fine\"; u - t < .02\r\n", "fine-1 ", None),
    # C port, the tree: a line's syntax error left its nodes taken; 340 such lines and the next
    # line's expression was Out of memory. The run reports the first line's error.
    "c_tree_leak": ("IF 0 THEN a = 1 + (2 * (3 +\r\n" * 400 + "PRINT 5\r\n", "Syntax error", "memory"),
    # C port, the tree: a field and an element by reference (P.REF), VARPTR of a field. The golden
    # corpus has neither by reference; the field passed at the wrong offset changed nothing it read.
    "c_ref_args": ("TYPE T\r\na AS INTEGER\r\nb AS INTEGER\r\nEND TYPE\r\nDECLARE SUB inc (x AS INTEGER)\r\n"
                   "DIM r AS T, v(3) AS INTEGER\r\ninc r.b: inc v(2): CALL inc(r.b)\r\n"
                   "PRINT r.b; v(2); VARPTR(r.b) - VARPTR(r)\r\nSUB inc (x AS INTEGER)\r\nx = x + 1\r\nEND SUB\r\n",
                   " 2  1  4 \r\n", "ERROR"),
    # A procedure DECLAREd only was ERROR 35 or 18 when its call ran: these printed "ran", then the
    # error, or nothing; the TYPE argument's was Type mismatch. BC rejects the program first.
    "c_undef_function": ('DECLARE FUNCTION g% (x%)\r\nPRINT "ran"\r\n30 PRINT 1 + g%(2)\r\n',
                         "Function not defined in line 30\r\n", "ran"),
    "qb_snd_bare": ('DECLARE SUB S (x AS INTEGER)\r\nPRINT "ran"\r\n20 S 1\r\n', "Subprogram not defined in line 20\r\n", "ran"),
    "qb_snd_call": ('DECLARE SUB S (x AS INTEGER)\r\nPRINT "ran"\r\n20 CALL S(1)\r\n', "Subprogram not defined in line 20\r\n", "ran"),
    "qb_snd_record": ("TYPE T\r\nv AS INTEGER\r\nEND TYPE\r\nDECLARE SUB S (g AS T, a() AS INTEGER)\r\n"
                      'DIM g AS T, a(3)\r\nPRINT "ran"\r\n20 S g, a()\r\n', "Subprogram not defined in line 20\r\n", "ran"),
    "qb_snd_uncalled": ('DECLARE SUB S ()\r\nPRINT "ran"\r\nSUB G\r\n20 IF 0 THEN S\r\nEND SUB\r\n',
                        "Subprogram not defined in line 20\r\n", "ran"),
    # C port: llrm-cc compiled REDIM's case of decl_line's switch (shared with DIM) past its load of
    # decl.proc, so REDIM bound the array in a garbage scope: UBOUND after it was Array not defined.
    "c_redim_ubound": ("REDIM v(3)\r\nPRINT UBOUND(v); LBOUND(v)\r\n", " 3  0 \r\n", "ERROR"),
    # C port: the same miscompile in blok_pair: WEND (sharing LOOP's case) read a stale block depth,
    # so every WEND on a line of its own was WEND without WHILE.
    "c_wend_line": ("WHILE i < 2\r\ni = i + 1\r\nWEND\r\nDO\r\nj = j + 1\r\nLOOP UNTIL j = 3\r\nPRINT i; j\r\n",
                    " 2  3 \r\n", "ERROR"),
    # C port: a string literal left the cursor at its text, so the ':' after it was a Syntax error.
    "c_str_colon": ('X$="HI":PRINT X$\r\n', "HI\r\n", "ERROR"),
    # C port: the same miscompile: END FUNCTION and END DEF (sharing END SUB's case) went to
    # "END SUB/FUNCTION without SUB/FUNCTION" without checking for an open procedure.
    "c_end_function": ("10 DEF FNT$(A$)\r\n20 FNT$ = A$ + \"!\"\r\n30 END DEF\r\n40 PRINT F%(3); FNT$(\"X\")\r\n"
                       "50 FUNCTION F%(A%)\r\n60 F% = A% * 2\r\n70 END FUNCTION\r\n", " 6 X!\r\n", "ERROR"),
    # ctail's inc esi after the and: ZF was the inc's, so a false IF ran the
    # rest of the line.
    "cmpif_false": ('A=5:IF A=4 THEN PRINT "NO":PRINT "NO"\r\nPRINT "END"\r\n', "END\r\n", "NO"),
    # cm_p's stack check kept the 16-bit room while each level took 12 bytes,
    # not 6: 14 parentheses were OUT OF MEMORY.
    "paren_depth": ("PRINT " + "(" * 29 + "7" + ")" * 29 + "\r\n", " 7 \r\n", "ERROR"),
    # The lexer took a name after an A5h byte (spaces skipped) for FN's: 42405
    # is M_INT A5h A5h, so A was FN A.
    "fn_lookback": ("PRINT 42405; A\r\n", " 42405  0 \r\n", "ERROR"),
    # INT was 16-bit: these overflowed at 32767.
    "int32_add": ("A%=32767:A%=A%+1:PRINT A%\r\n", " 32768 \r\n", "ERROR"),
    "int32_mul": ("A%=-70000:PRINT 300*300;A%*30000\r\n", " 90000 -2100000000 \r\n", "ERROR"),
    "int32_arr": ("DIM B%(3):B%(1)=70000:B%(2)=-5:PRINT B%(1);B%(2)\r\n", " 70000 -5 \r\n", "ERROR"),
    "int32_for": ("FOR I%=32766 TO 32768:PRINT I%;:NEXT\r\n", " 32766  32767  32768 ", "ERROR"),
    "int32_ovf": ("A%=65536:PRINT A%*32768\r\n", "Overflow in line", None),
    "int32_toi": ("A=2^30:A%=A*2-128:PRINT A%:A%=-A*2:PRINT A%:A%=A*2\r\n",
                  " 2147483520 \r\n-2147483648 \r\nIllegal function call in line", None),
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
    # INPUT's end was 16 bits of a cache offset: past 64 KB of code an empty reply went elsewhere.
    "qb_input_far": (("".join("b = 1" + "".join(f": b = b * 3 + {k}" for k in range(20)) + "\r\n" for _ in range(600))
                      + "INPUT x, y: PRINT 1\r\nPRINT 2\r\n", "\r\n"), "? \r\n 1 \r\n 2 \r\n", "ERROR"),
    # RESTORE took a line number only: a label was ?UNDEF'D STATEMENT.
    "qb_restore": ("READ a\r\nRESTORE two\r\nREAD b\r\nPRINT a; b\r\nDATA 1\r\ntwo:\r\nDATA 2\r\n",
                   " 1  2 \r\n", "ERROR"),
    # An error on a line with no number said IN 65535, NONUM.
    "qb_err_nonum": ("PRINT 1/0\r\n", "Division by zero in line No line number in module", "65535"),
    # INT / INT truncated, C64-BC style: QBasic's / is a FLOAT.
    "qb_div": ("A%=7:PRINT A%/2;7/2\r\n", " 3.5  3.5 \r\n", "ERROR"),
    # DIM AS, DEFtype and CONST were syntax errors.
    "qb_decl": ("DEFINT A-Z\r\nDIM s AS STRING, v AS SINGLE: DIM x(5) AS LONG\r\nCONST N = 4, PI = 3.5\r\n"
                "CONST T$ = \"t\"\r\na = 7 / 2: s = \"hi\": v = 7 / 2: x(2) = 70000\r\nPRINT a; s; v; x(2); PI; T$; N\r\n",
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
                  "PRINT LEN(b); LEN(b.t); PEEK(VARPTR(b)); PEEK(VARPTR(b) + 1); "
                  "PEEK(VARPTR(b.t) + 2)\r\nPRINT b.x; b.y; b.t; \"|\"\r\n",
                  " 9  3  52  18  32 \r\n 4660 -1 ab |\r\n", "ERROR"),
    "qb_short": ("TYPE V\r\nx AS SHORT\r\nEND TYPE\r\nDIM a AS V: a.x = -32768: PRINT a.x: a.x = 32768\r\n",
                 "-32768 \r\nOverflow in line", None),
    # DIM v AS a TYPE skipped two bytes of the TYPE's name: SYNTAX ERROR. rc_ref lost a(i)'s
    # '(' to var_find's EDX: SYNTAX ERROR.
    "qb_type": ("TYPE P\r\nx AS LONG\r\nt AS STRING * 2\r\nEND TYPE\r\nTYPE L\r\na AS P\r\nb AS P\r\nEND TYPE\r\n"
                'DIM c AS L, d AS L, q(3) AS P\r\nc.a.x = 5: c.b.t = "xyz": d = c: c.a.x = 0\r\n'
                "q(2).x = 9: q(1).x = q(2).x + 1\r\nPRINT d.a.x; d.b.t; q(1).x; q(2).x\r\n",
                " 5 xy 10  9 \r\n", "ERROR"),
    "int32_lit": ("A%=65535:PRINT A%*2\r\n", " 131070 \r\n", "ERROR"),
    # Flat: PEEK and POKE take VARPTR's and SADD's 4-byte address; VARSEG and SSEG are 0.
    "qb_flat": ('DIM n AS LONG, a(3) AS LONG: a(2) = 7: b$ = "HE" + "LLO"\r\n'
                "POKE VARPTR(n) + 1, 1: POKE SADD(b$), 74\r\n"
                "PRINT n; PEEK(VARPTR(a(2))); b$; PEEK(SADD(b$) + 4); VARSEG(n); SSEG(b$)\r\n",
                " 256  7 JELLO 79  0  0 \r\n", "ERROR"),
    # SADD and SSEG take a variable-length STRING alone: SADD of a SINGLE's 1 gave 16256, a record's
    # 0, a STRING * n's 16777206; SSEG of a number 0.
    **{f"qb_{fn.lower()}_{k}": (f"{d}\r\nPRINT {fn}({v})\r\n", f"^\r\n`{v}` is not a variable-length STRING", None)
       for fn, k, d, v in (("SADD", "num", "x = 1", "x"), ("SADD", "fixed", "DIM f AS STRING * 4", "f"),
                           ("SADD", "record", "TYPE P\r\n a AS INTEGER\r\nEND TYPE\r\nDIM r AS P", "r"),
                           ("SSEG", "num", "x = 1", "x"))},
    # ON ERROR: the handler runs on the failing procedure's frames; RESUME NEXT goes on inside
    # the FUNCTION a statement called mid-expression.
    "qb_onerr": ("DECLARE FUNCTION f% (d%)\r\nON ERROR GOTO h\r\nPRINT 1 + f(0); ERR\r\n"
                 "k = -1: s$ = SPACE$(k): PRINT LEN(s$): ERROR 7\r\nPRINT \"no\"\r\nx:\r\nPRINT \"x\"\r\nEND\r\n"
                 "h: PRINT \"e\"; ERR; ERL;\r\nIF ERR = 5 THEN k = 2: RESUME 0\r\n"
                 "IF ERR = 7 THEN RESUME x\r\nRESUME NEXT\r\n"
                 "FUNCTION f% (d%)\r\n50 c% = 10 \\ d%: f% = c% + 4\r\nEND FUNCTION\r\n",
                 "e 11  50  5  0 \r\ne 5  0  2 \r\ne 7  0 x\r\n", "ERROR"),
    # A DOUBLE element of 2 or more subscripts stored without its count: the VM ran its operands.
    "qb_dbl_arr2": ('DIM g#(9, 9)\r\ng#(1, 2) = 3\r\nPRINT g#(1, 2); g#(2, 1)\r\n', " 3  0 \r\n", "ERROR"),
    # Freeing an array walked all 4096 ids, not those in use: 4000 calls of a SUB with local arrays
    # took 15 s, past the limit.
    "qb_dimfree": ('DECLARE SUB a (n AS INTEGER)\r\nFOR i = 1 TO 4000: a i MOD 50: NEXT: PRINT "ok"\r\nEND\r\n'
                   'SUB a (n AS INTEGER)\r\nREDIM x(300 + n) AS INTEGER\r\nDIM y(40) AS LONG\r\nEND SUB\r\n',
                   "ok\r\n", "ERROR"),
    # A line's tokens were 255 bytes at most, Out of memory past; positions and the statement map
    # held body offsets in a byte: a loop's or RESUME's statement past 255 was lost.
    "qb_long_line": ('ON ERROR GOTO h\r\nDO: i = i + 1: a0 = 1: a1 = 1: a2 = 1: a3 = 1: a4 = 1: a5 = 1: a6 = 1: a7 = 1: a8 = 1: a9 = 1: a10 = 1: a11 = 1: a12 = 1: a13 = 1: a14 = 1: a15 = 1: a16 = 1: a17 = 1: a18 = 1: a19 = 1: a20 = 1: a21 = 1: a22 = 1: a23 = 1: a24 = 1: LOOP UNTIL i = 3: PRINT i\r\n'
                     'a0 = 1: a1 = 1: a2 = 1: a3 = 1: a4 = 1: a5 = 1: a6 = 1: a7 = 1: a8 = 1: a9 = 1: a10 = 1: a11 = 1: a12 = 1: a13 = 1: a14 = 1: a15 = 1: a16 = 1: a17 = 1: a18 = 1: a19 = 1: a20 = 1: a21 = 1: a22 = 1: a23 = 1: a24 = 1: x = 1 / 0: PRINT "n"\r\nEND\r\nh: RESUME NEXT\r\n', " 3 \r\nn\r\n", "ERROR"),
    # PDS's ON LOCAL ERROR was a syntax error. Its handler takes a callee's error, the callee
    # dropped: RESUME NEXT goes on after the call; it ends with its procedure.
    "qb_onlocal": ('DECLARE SUB t ()\r\nDECLARE SUB m ()\r\nt\r\nPRINT "d": x = 1 / 0: PRINT "no"\r\n'
                   'SUB t\r\nON LOCAL ERROR GOTO h\r\nm\r\nPRINT "t"\r\nEXIT SUB\r\nh: PRINT "e"; ERR: RESUME NEXT\r\n'
                   'END SUB\r\nSUB m\r\nx = 1 / 0: PRINT "m"\r\nEND SUB\r\n',
                   "e 11 \r\nt\r\nd\r\nDivision by zero", "\nno\r"),
    # PDS's REDIM PRESERVE was a syntax error. Elements keep their subscripts; strings move with them;
    # another array after it stays.
    "qb_redim_keep": ('REDIM a(3) AS INTEGER, s$(2), z(1)\r\nFOR i = 0 TO 3: a(i) = i * 10: NEXT\r\n'
                      's$(0) = "x": s$(1) = "yy": s$(2) = "zzz": z(1) = 4\r\nREDIM PRESERVE a(5) AS INTEGER\r\n'
                      'REDIM PRESERVE s$(1 TO 4)\r\nFOR i = 0 TO 5: PRINT a(i); : NEXT: PRINT\r\n'
                      'FOR i = 1 TO 4: PRINT s$(i); ","; : NEXT: PRINT\r\nREDIM b(1, 2)\r\n'
                      'b(0, 0) = 1: b(0, 2) = 3: b(1, 1) = 5\r\nREDIM PRESERVE b(2, 3)\r\n'
                      'PRINT b(0, 0); b(0, 2); b(1, 1); b(2, 3); z(1)\r\nREDIM PRESERVE a(1)\r\nPRINT a(0); a(1); UBOUND(a)\r\n',
                      " 0  10  20  30  0  0 \r\nyy,zzz,,,\r\n 1  3  5  0  4 \r\n 0  10  1 \r\n", "ERROR"),
    # SHORT was an INT outside a record. QBasic's INTEGER: SHORT arithmetic overflows past 16 bits,
    # a store to one too; with an INT or a FLOAT it widens; HEX$ shows 16 bits.
    "qb_short_ops": ('DIM s AS SHORT, t AS SHORT\r\nON ERROR GOTO h\r\ns = 32767: t = -1\r\n'
                     'PRINT s + 40000; s + 1.5; HEX$(t); HEX$(-1)\r\nPRINT s + 1\r\ns = 40000\r\n'
                     'DIM a(3) AS SHORT\r\na(1) = s: a(2) = a(1) * 2\r\nREAD t\r\nPRINT s - 1; a(1) \\ 2 + 5\r\n'
                     'END\r\nDATA 32768\r\nh: PRINT "ov"; ERR: RESUME NEXT\r\n',
                     " 72767  32768.5 FFFFFFFFFFFF\r\nov 6 \r\nov 6 \r\nov 6 \r\nov 6 \r\n 32766  16388 \r\n", "ERROR"),
    # INP and OUT were syntax errors; WAIT read POKE's memory, not a port. The DAC's registers read
    # back what OUT wrote; WAIT ends on the vertical retrace's bit.
    "qb_ports": ('OUT &H3C8, 5: OUT &H3C9, 10: OUT &H3C9, 20: OUT &H3C9, 30\r\n'
                 'OUT &H3C7, 5: PRINT INP(&H3C9); INP(&H3C9); INP(&H3C9)\r\nWAIT &H3DA, 8: WAIT &H3DA, 8, 8: PRINT "w"\r\n',
                 " 10  20  30 \r\nw\r\n", "ERROR"),
    # SHELL was a syntax error. The child gets ENVIRON's environment and writes after what was
    # printed; a COMSPEC that does not run is Illegal function call.
    "qb_shell": ('PRINT "a"\r\nENVIRON "QBX=hello"\r\nSHELL "set"\r\nSHELL "echo b"\r\nPRINT "c"\r\nON ERROR GOTO h\r\n'
                 'ENVIRON "COMSPEC=X:\\NONE.COM"\r\nSHELL "echo d"\r\nEND\r\nh: PRINT "e"; ERR: RESUME NEXT\r\n',
                 "QBX=hello\r\nb\r\nc\r\ne 5 \r\n", "warning"),
    # Event trapping was a syntax error. ON TIMER runs its GOSUB each interval while ON; OFF drops
    # events; STOP keeps one for ON, run at the next jump or line's end. A key ON KEY traps (here pushed through the 8042) is taken.
    "qb_events": ('ON TIMER(1) GOSUB tk\r\nTIMER ON\r\nt0 = TIMER: DO: LOOP UNTIL n >= 2 OR TIMER - t0 > 5\r\n'
                  'TIMER OFF: PRINT "n"; n\r\nt0 = TIMER: DO: LOOP UNTIL TIMER - t0 > 1.3\r\nPRINT "off"; n\r\n'
                  'TIMER STOP: t0 = TIMER: DO: LOOP UNTIL TIMER - t0 > 1.3\r\nPRINT "stop"; n\r\nTIMER ON\r\n'
                  'TIMER OFF: PRINT "on"; n\r\nON KEY(1) GOSUB f1\r\nKEY(1) ON\r\nOUT &H64, &HD2: OUT &H60, &H3B\r\n'
                  't0 = TIMER: DO: LOOP UNTIL k OR TIMER - t0 > 2\r\nPRINT "key"; k; INKEY$ = ""\r\nEND\r\n'
                  'tk: n = n + 1: RETURN\r\nf1: k = k + 1: RETURN\r\n',
                  "n 2 \r\noff 2 \r\nstop 2 \r\non 3 \r\nkey 1 -1 \r\n", "ERROR"),
    # CHAIN and COMMON were syntax errors. COMMON's values pass by position to CHAIN's file (FILES'
    # QB_CH2.BAS), arrays with their bounds; files stay open; a missing file is File not found.
    "qb_chain": ('COMMON a%, s$, d#, x(), t AS SHORT\r\nDIM x(1 TO 3)\r\na% = 7: s$ = "hi": d# = 1.25: x(3) = 9: t = -2\r\n'
                 'OPEN "QB_CH.TXT" FOR OUTPUT AS #1\r\nPRINT "a"\r\nCHAIN "QB_CH2"\r\nPRINT "no"\r\n',
                 "a\r\n 7 hi 1.25  9  1 -2  0 \r\ne 53 \r\n", "no"),
    # COMMON's values past the CHAIN buffer's first bytes: it grows.
    "qb_chainbig": ('COMMON s$, x()\r\nDIM x(600)\r\nx(500) = 5: s$ = STRING$(3000, "z")\r\nCHAIN "QB_CH3"\r\n',
                    " 3000 z 5 \r\n", None),
    # 512 COMMON variables and 64 KB of values at most: past either, CHAIN was Out of memory.
    "qb_chain_many": (f"COMMON {', '.join(f'c{i}' for i in range(600))}, x() AS LONG\r\nDIM x(30000) AS LONG\r\n"
                      'c0 = 1: c599 = 599: x(30000) = 7\r\nCHAIN "QB_CH5"\r\n', " 1  599  7 \r\n", "ERROR"),
    # Declarations were by name, program-wide: a module's DIM AS typed a SUB's local, a SUB's DIM AS
    # and CONST the module's and other SUBs' (Type mismatch; the module's k became 2), and a GOSUB
    # took another procedure's label. Each scope is now its own, the module's seen when shared.
    "qb_scopes": ('DECLARE SUB s ()\r\nDECLARE SUB t ()\r\nDIM a AS STRING: CONST k = 1\r\nDIM SHARED z: z = 7: y = 6\r\n'
                  'a = "m": PRINT a\r\ns\r\nt\r\nb = 2: PRINT b; k; y\r\nGOSUB l\r\nEND\r\nl: PRINT "m": RETURN\r\n'
                  'SUB s\r\nSHARED y\r\nCONST k = 2\r\nDIM b AS STRING\r\na = 5: b = "q": PRINT a; b; k; z\r\ny = 8\r\n'
                  'GOSUB l2\r\nEXIT SUB\r\nl2: PRINT "s": RETURN\r\nEND SUB\r\nSUB t\r\nb = 4: PRINT b; y\r\nEND SUB\r\n',
                  "m\r\n 5 q 2  7 \r\ns\r\n 4  0 \r\n 2  1  8 \r\nm\r\n", "ERROR"),
    # Scope errors QB45, PDS 7.1 and VBDOS's BC give, where these ran: a label is the module's
    # name, one in all scopes; a suffix other than a name's AS type; assigning a SUB, or a
    # FUNCTION outside it; CONST, DIM in a SUB of a DIM SHARED name; DIM AS after a use.
    "qb_dlabel": ('DECLARE SUB s ()\r\ns\r\nGOSUB l\r\nEND\r\nl: PRINT "m": RETURN\r\n'
                  'SUB s\r\nGOSUB l\r\nEXIT SUB\r\nl: PRINT "s": RETURN\r\nEND SUB\r\n', "Duplicate label", None),
    "qb_dlabel2": ('DECLARE SUB s ()\r\nDECLARE SUB t ()\r\ns\r\nt\r\nSUB s\r\nl: PRINT "s"\r\nEND SUB\r\n'
                   'SUB t\r\nl: PRINT "t"\r\nEND SUB\r\n', "Duplicate label", None),
    "qb_sfxdup": ("DIM x AS INTEGER\r\nx! = 2\r\nPRINT x\r\n", "Duplicate definition", None),
    "qb_sfxdupa": ("DIM x(3) AS INTEGER\r\nx!(1) = 2: PRINT x(1)\r\n", "Duplicate definition", None),
    "qb_subasg": ('DECLARE SUB s ()\r\ns\r\nSUB s\r\nPRINT "in"\r\ns = 1\r\nEND SUB\r\n', "Syntax error", "Out of stack"),
    "qb_fnasg": ("DECLARE FUNCTION f ()\r\nf = 3\r\nPRINT f\r\nEND\r\nFUNCTION f\r\nf = 1\r\nEND FUNCTION\r\n",
                 "Variable required", None),
    "qb_shrconst": ("DECLARE SUB s ()\r\nDIM SHARED x\r\nx = 1: s: PRINT x\r\nSUB s\r\nCONST x = 4\r\nPRINT x\r\nEND SUB\r\n",
                    "Syntax error", None),
    "qb_shrdim": ("DECLARE SUB s ()\r\nDIM SHARED x\r\nx = 1\r\ns\r\nPRINT x\r\nSUB s\r\nDIM x\r\nx = 5: PRINT x\r\nEND SUB\r\n",
                  "Duplicate definition", None),
    "qb_asfirst": ("x% = 1\r\nDIM x AS INTEGER\r\nPRINT x\r\n", "AS clause required on first declaration", None),
    # RESTORE's label in a SUB looked in the SUB's labels, not the module's; a CONST with another
    # suffix, or none, was another variable: 0.
    "qb_restsub": ('DECLARE SUB s ()\r\ns\r\nEND\r\nd: DATA 7\r\nSUB s\r\nRESTORE d\r\nREAD a: PRINT a\r\nEND SUB\r\n',
                   " 7 \r\n", "ERROR"),
    "qb_constsfx": ("CONST k = 1, j% = 2\r\nPRINT k%; j\r\n", " 1  2 \r\n", "ERROR"),
    # A CONST of a DOUBLE's literal (# or past 7 digits) was a SINGLE: 6.283185, and a vine 7498 turns on
    # drawn 1.3 ms of angle off.
    "c_const_double": ("CONST G = 6.283185307179586#, H = 3.14159265358979\r\nPRINT G; H\r\n",
                       " 6.283185307179586  3.14159265358979 \r\n", "ERROR"),
    # A procedure's FOR on its own locals is one op, I.NEXT's frame twin: up, down, nested, each call's own.
    "c_next_local": ("DECLARE FUNCTION r% (n%)\r\nPRINT r%(3)\r\nFUNCTION r% (n%)\r\nDIM i%, j%, s%\r\n"
                     "FOR i% = 1 TO n%: s% = s% + i%: NEXT\r\nFOR i% = n% TO 1 STEP -2: s% = s% + i%: NEXT\r\n"
                     "FOR i% = 1 TO 2: FOR j% = i% TO 2: s% = s% + j%: NEXT j%, i%\r\n"
                     "IF n% > 1 THEN s% = s% + r%(n% - 1)\r\nPRINT i%; j%;\r\nr% = s%\r\nEND FUNCTION\r\n",
                     " 3  3  3  3  3  3  32 \r\n", "ERROR"),
    # A line number is a label: the same twice, in any scopes, ran both lines; a SUB's GOSUB
    # to the module's ran it.
    "qb_dupnum": ("10 PRINT 1\r\n10 PRINT 2\r\n", "Duplicate label", None),
    "qb_dupnum2": ('DECLARE SUB s ()\r\n10 s\r\nSUB s\r\n10 PRINT "s"\r\nEND SUB\r\n', "Duplicate label", None),
    "qb_numsc": ('DECLARE SUB s ()\r\ns\r\nEND\r\n20 PRINT "m": RETURN\r\nSUB s\r\nGOSUB 20\r\nEND SUB\r\n',
                 "Subprogram error", None),
    "qb_numsub": ('DECLARE SUB s ()\r\ns\r\nGOTO 20\r\n20 PRINT "m"\r\nSUB s\r\nGOSUB 30\r\nEXIT SUB\r\n'
                  '30 PRINT "s": RETURN\r\nEND SUB\r\n', "s\r\nm\r\n", "ERROR"),
    # DEFtype held for the whole program; BC's from its line to the file's end, SUBs too.
    "qb_deftpos": ("a = 1.5: PRINT a\r\nDEFINT A\r\na = 2.7: PRINT a; a!\r\n", " 1.5 \r\n 3  1.5 \r\n", "ERROR"),
    "qb_deftsub": ("DECLARE SUB s ()\r\nDECLARE SUB t ()\r\ns: t: c = 1.5: PRINT c\r\nSUB s\r\nDEFINT C\r\nEND SUB\r\n"
                   "SUB t\r\nc = 1.5: PRINT c\r\nEND SUB\r\n", " 2 \r\n 1.5 \r\n", "ERROR"),
    # OPTION BASE: an undimensioned array's was 0; after a DIM, again, or not 0 or 1 it ran.
    "qb_obimp": ("OPTION BASE 1\r\nb(2) = 1: PRINT LBOUND(b); UBOUND(b)\r\n", " 1  10 \r\n", "ERROR"),
    "qb_obdim": ("DIM a(3)\r\nOPTION BASE 1\r\nPRINT LBOUND(a)\r\n", "Array already dimensioned", None),
    "qb_obtwice": ("OPTION BASE 1\r\nOPTION BASE 0\r\nDIM a(3): PRINT LBOUND(a)\r\n", "Syntax error", None),
    "qb_ob2": ("OPTION BASE 2\r\nPRINT 1\r\n", "Syntax error", None),
    # VBDOS's OPTION EXPLICIT was ignored: every variable, array, FOR's too, must be declared.
    "qb_oexp": ("OPTION EXPLICIT\r\nDIM x\r\nx = 1: y = 2: PRINT x\r\n", "Variable not declared", None),
    "qb_oexpfor": ("OPTION EXPLICIT\r\nDIM x\r\nFOR i = 1 TO 2: NEXT\r\n", "Variable not declared", None),
    "qb_oexparr": ("OPTION EXPLICIT\r\nDIM a(3)\r\nb(1) = 2\r\n", "Variable not declared", None),
    "qb_oexpok": ("OPTION EXPLICIT\r\nDECLARE SUB s (a)\r\nDECLARE FUNCTION f% ()\r\nCONST k = 2\r\nDIM SHARED g\r\n"
                  "DIM i AS INTEGER, v(2)\r\nFOR i = 1 TO 1: v(i) = f%: NEXT\r\ns k + v(1)\r\n"
                  "SUB s (a)\r\nDIM l\r\nl = a + g: PRINT l\r\nEND SUB\r\nFUNCTION f%\r\nSTATIC n\r\nn = n + 1: f% = n\r\nEND FUNCTION\r\n",
                  " 3 \r\n", "ERROR"),
    # An array's element, a record's field went by value; a variable, an array of another type
    # passed converted; BYVAL (PDS's) was by reference.
    "qb_refel": ("DECLARE SUB s (a)\r\nDECLARE SUB t (b$)\r\nDIM v(3), w$(2): v(2) = 1\r\n"
                 "s v(2): PRINT v(2)\r\nCALL s(v(2)): PRINT v(2)\r\nt w$(1): PRINT w$(1)\r\n"
                 "SUB s (a)\r\nDIM l(500): a = a + 1\r\nEND SUB\r\nSUB t (b$)\r\nb$ = b$ + \"q\"\r\nEND SUB\r\n",
                 " 2 \r\n 3 \r\nq\r\n", "ERROR"),
    "qb_reffld": ("TYPE t\r\n f AS INTEGER\r\n d AS DOUBLE\r\nEND TYPE\r\nDECLARE SUB s (a%)\r\nDECLARE SUB u (b#)\r\n"
                  "DIM r AS t, q(2) AS t\r\ns r.f: s q(1).f: u q(2).d: PRINT r.f; q(1).f; q(2).d\r\n"
                  "SUB s (a%)\r\na% = 9\r\nEND SUB\r\nSUB u (b#)\r\nb# = 2.25\r\nEND SUB\r\n",
                  " 9  9  2.25 \r\n", "ERROR"),
    "qb_ptmv": ("DECLARE SUB s (a%)\r\nx! = 1: s x!: PRINT x!\r\nSUB s (a%)\r\na% = 5\r\nEND SUB\r\n",
                "Parameter type mismatch", None),
    "qb_ptma": ("DECLARE SUB s (a%())\r\nDIM v(3)\r\ns v()\r\nSUB s (a%())\r\nPRINT \"in\"\r\nEND SUB\r\n",
                "Parameter type mismatch", None),
    # A compile error was an ERR op raised only when its line ran: these printed "ran", then the
    # error, or never. BC rejects the program first.
    "qb_ptm_late": ('DECLARE FUNCTION F% (a AS SINGLE)\r\nDECLARE SUB G ()\r\nPRINT "ran"\r\nG\r\nSUB G\r\n'
                    "DIM t AS INTEGER\r\n70 IF 0 THEN IF F%(t) THEN PRINT 1\r\nEND SUB\r\n"
                    "FUNCTION F% (a AS SINGLE)\r\nEND FUNCTION\r\n",
                    "Parameter type mismatch in line 70\r\n", "ran"),
    "qb_ptm_uncalled": ('DECLARE FUNCTION F% (a AS SINGLE)\r\nPRINT "ran"\r\nSUB G\r\nDIM t AS INTEGER\r\n'
                        "60 x = F%(t)\r\nEND SUB\r\nFUNCTION F% (a AS SINGLE)\r\nEND FUNCTION\r\n",
                        "Parameter type mismatch in line 60\r\n", "ran"),
    "qb_tm_uncalled": ('PRINT "ran"\r\nSUB G\r\n40 x% = "a"\r\nEND SUB\r\n', "Type mismatch in line 40\r\n", "ran"),
    # A variable under a CONST's name, any case or suffix, was the CONST: stores to it were
    # dropped and these printed 5. BC: Duplicate definition.
    "qb_rd_const_dim": ("CONST LIMIT = 5\r\n20 DIM SHARED limit AS SINGLE\r\nlimit = 9\r\nPRINT limit\r\n",
                        "Duplicate definition in line 20\r\n", " 5"),
    "qb_rd_const_let": ("CONST LIMIT = 5\r\n20 limit = 9\r\nPRINT limit\r\n", "Duplicate definition in line 20\r\n", " 5"),
    "qb_rd_const_sfx": ("CONST LIMIT = 5\r\n20 limit! = 9\r\nPRINT limit\r\n", "Duplicate definition in line 20\r\n", " 5"),
    "qb_rd_const_for": ("CONST LIMIT = 5\r\n20 FOR limit = 1 TO 2: NEXT\r\n", "Duplicate definition in line 20\r\n", None),
    "qb_rd_const_input": ("CONST LIMIT = 5\r\n20 INPUT limit\r\n", "Duplicate definition in line 20\r\n", "?"),
    "qb_rd_const_dimafter": ("20 DIM limit\r\nCONST LIMIT = 5\r\n", "Duplicate definition in line 20\r\n", None),
    "qb_rd_const_param": ("DECLARE SUB s (limit)\r\nCONST LIMIT = 5\r\ns 2\r\n20 SUB s (limit)\r\nEND SUB\r\n",
                          "Duplicate definition in line 20\r\n", None),
    "qb_rd_const_sub": ("DECLARE SUB s ()\r\nCONST LIMIT = 5\r\ns\r\nSUB s\r\n20 STATIC limit\r\n"
                        "30 DIM limit\r\nEND SUB\r\n", "Duplicate definition in line 20\r\n", None),
    # The CONST as a value, and a procedure's CONST beside the module's variable, still work.
    "qb_const_ok": ("DECLARE SUB s ()\r\nCONST LIMIT = 5\r\nDIM b AS SINGLE, lim\r\nb = LIMIT * 2: lim = 7\r\n"
                    "PRINT b; LIMIT; lim\r\ns\r\nSUB s\r\nCONST LIM = 3\r\nPRINT LIM; LIMIT\r\nEND SUB\r\n",
                    " 10  5  7 \r\n 3  5 \r\n", "ERROR"),
    # A local array's bound written as a CONST expression (N + 100) was not constant: the array
    # took the heap (FRE(-1) fell) and each return's arry_free walked every live string, ~20 us a
    # call with 3000 strings. It gets a frame slot, as DIM x(200) does.
    "c_dim_const_expr": ("DECLARE SUB A (f AS LONG)\r\nCONST N = 100\r\nA FRE(-1)\r\nSUB A (f AS LONG)\r\n"
                         "DIM x(N + 100) AS INTEGER, y(-N \\ 2 TO N * 2 - 1) AS LONG, z(N / 3) AS INTEGER\r\n"
                         "x(200) = 7: y(-50) = 9\r\n"
                         "PRINT FRE(-1) = f; UBOUND(x); LBOUND(y); UBOUND(y); UBOUND(z); x(200); y(-50)\r\nEND SUB\r\n",
                         "-1  200 -50  199  33  7  9 \r\n", "ERROR"),
    # A FUNCTION or SUB with parameters called with none: its frame read the caller's cells as its
    # argument's address. Ang's parameter dy was DY%() and printed 1.073742E+09 (2's bits + 1); in
    # a big program the read segfaulted. A variable's name: Duplicate definition, saying why.
    "qb_rd_function": ("DECLARE FUNCTION Ang! (dx AS SINGLE, dy AS SINGLE)\r\nDECLARE FUNCTION DY% (d AS INTEGER)\r\n"
                       "PRINT Ang!(1, 2)\r\nFUNCTION Ang! (dx AS SINGLE, dy AS SINGLE)\r\n20 Ang! = dy / dx\r\n"
                       "END FUNCTION\r\nFUNCTION DY% (d AS INTEGER)\r\nDY% = d + 1\r\nEND FUNCTION\r\n",
                       "`dy` is also the name of a FUNCTION", "E+"),
    # The same, the callee's parameter an array: LandPix%'s sides was Sides%(), whose m() was the
    # caller's cells: Subscript out of range at an unrelated line; here 0.
    "qb_rd_function_array": ("DECLARE FUNCTION Sides% (m() AS INTEGER, x AS INTEGER)\r\nDECLARE FUNCTION Lp% (sides AS INTEGER)\r\n"
                             "PRINT Lp%(3)\r\nFUNCTION Lp% (sides AS INTEGER)\r\n20 Lp% = sides AND 1\r\nEND FUNCTION\r\n"
                             "FUNCTION Sides% (m() AS INTEGER, x AS INTEGER)\r\nSides% = m(x)\r\nEND FUNCTION\r\n",
                             "`Sides` is also the name of a FUNCTION", " 0"),
    "qb_acm_sub": ("DECLARE SUB s (a)\r\n20 s\r\nSUB s (a)\r\nPRINT a\r\nEND SUB\r\n",
                   "Argument-count mismatch in line 20\r\n", " 0"),
    "qb_acm_call": ("DECLARE SUB s (a)\r\n20 CALL s\r\nSUB s (a)\r\nPRINT a\r\nEND SUB\r\n",
                    "Argument-count mismatch in line 20\r\n", " 0"),
    "qb_byval": ("DECLARE SUB s (BYVAL a)\r\nx = 1: s x: PRINT x\r\nSUB s (BYVAL a)\r\na = 5\r\nEND SUB\r\n",
                 " 1 \r\n", "ERROR"),
    # A SUB's REDIM of its array parameter after ERASE made one the caller never saw.
    "qb_redimpar": ("DECLARE SUB s (a())\r\nREDIM v(3)\r\ns v(): PRINT UBOUND(v); v(9)\r\n"
                    "SUB s (a())\r\nREDIM a(9): a(9) = 4\r\nERASE a\r\nREDIM a(9): a(9) = 6\r\nEND SUB\r\n", " 9  6 \r\n", "ERROR"),
    # UBOUND of an ERASEd dynamic array was 10, an implicit one's; LBOUND of one not yet made 0
    # under OPTION BASE 1.
    "qb_ubnderase": ("REDIM v(3)\r\nERASE v\r\nPRINT UBOUND(v)\r\n", "Subscript out of range", None),
    "qb_lbndimp": ("OPTION BASE 1\r\nIF 0 THEN w(1) = 1\r\nPRINT LBOUND(w); UBOUND(w)\r\n", " 1  10 \r\n", "ERROR"),
    # Parameters were copied in and out: a variable passed twice, or SHARED and passed, was two
    # inside the call, the last copy back winning.
    "qb_alias": ("DECLARE SUB s (a, b)\r\nx = 1: s x, x: PRINT x\r\nSUB s (a, b)\r\na = 5: PRINT b;\r\nEND SUB\r\n",
                 " 5  5 \r\n", "ERROR"),
    "qb_aliasshr": ("DECLARE SUB s (a$)\r\nDIM SHARED g$\r\ng$ = \"x\": s g$: PRINT g$\r\n"
                    "SUB s (a$)\r\na$ = \"y\": PRINT g$;\r\ng$ = \"z\": PRINT a$\r\nEND SUB\r\n", "yz\r\nz\r\n", "ERROR"),
    "qb_refnest": ("DECLARE SUB s (a#)\r\nDECLARE SUB t (b#)\r\nx# = 1: s x#: PRINT x#\r\n"
                   "SUB s (a#)\r\nt a#: a# = a# * 10\r\nEND SUB\r\nSUB t (b#)\r\nb# = b# + 1\r\nEND SUB\r\n", " 20 \r\n", "ERROR"),
    "qb_reffor": ("DECLARE SUB s (a)\r\ns x: PRINT x\r\nSUB s (a)\r\nFOR a = 1 TO 3: NEXT\r\nEND SUB\r\n", " 4 \r\n", "ERROR"),
    "qb_refmove": ("DECLARE SUB s (a, b$)\r\nREDIM SHARED t(100)\r\nREDIM v(3), w$(3)\r\ns v(2), w$(1): PRINT v(2); w$(1)\r\n"
                   "SUB s (a, b$)\r\nERASE t\r\na = 7: b$ = \"m\"\r\nEND SUB\r\n", " 7 m\r\n", "ERROR"),
    # A SINGLE printed, and read, through repeated single-precision steps, rounded more than
    # once: 1/3 was .3333334, 9.999999 was 10, .001 9.999999E-04; past 7 digits it was not
    # E+nn; under 1 it went E-nn by its power, not by the decimals it needs, DOUBLE's too. ^ of
    # INTs was an INT; 12345678 a SINGLE.
    "qb_sfmt": ("PRINT 1 / 3; 9.999999; 99999.99; 1E7; 12345678; 2 ^ 24\r\n"
                "PRINT .001; .00001; .0000001; 1E-08; 1.2E-6; 1.2E-7; 1.234567E-02; 1.23456E-03\r\n"
                "PRINT STR$(.9999999); STR$(5 / 9E8); STR$(1.5E-10)\r\n"
                "PRINT 1D-04; 1.5D-05; .000001#; 1D-16; 1.2D-16; 1.2345678D-08; 1.23456789012345D-02\r\n",
                " .3333333  9.999999  99999.99  1E+07  12345678  1.677722E+07 \r\n"
                " .001  .00001  .0000001  1E-08  .0000012  1.2E-07  1.234567E-02  1.23456E-03 \r\n"
                " .9999999 5.555556E-09 1.5E-10\r\n"
                " .0001  .000015  .000001  .0000000000000001  1.2D-16  .000000012345678  .0123456789012345 \r\n", "ERROR"),
    # STATIC's AS type was skipped: its names took their DEFtype, a string Type mismatch.
    "qb_static_as": ('DECLARE SUB s ()\r\nCALL s: s\r\nSUB s\r\nSTATIC n AS STRING, d AS DOUBLE\r\n'
                     'n = n + "a": d = d + .5: PRINT n; d\r\nEND SUB\r\n', "a .5 \r\naa 1 \r\n", "ERROR"),
    # DEFSHORT, ours, was a syntax error: its letters' names a SHORT, overflowing past 16 bits.
    "qb_defshort": ('DEFSHORT S-T\r\nON ERROR GOTO h\r\nu = 40000: s = 32767: PRINT u; s\r\nt = u\r\nEND\r\n'
                    'h: PRINT "ov"; ERR: RESUME NEXT\r\n', " 40000  32767 \r\nov 6 \r\n", "ERROR"),
    # UBOUND of an array not yet in the text of its scope gave an implicit array's 10; BC's:
    # Array not defined, at compile time, by the text's order, not the run's.
    "qb_andef": ('PRINT 1\r\nGOTO 10\r\n5 PRINT UBOUND(a)\r\nEND\r\n10 a(1) = 1: GOTO 5\r\n',
                 "Array not defined", " 1"),
    "qb_andefsub": ('DECLARE SUB s ()\r\nDIM a(5)\r\ns\r\nSUB s\r\nPRINT LBOUND(a)\r\nEND SUB\r\n',
                    "Array not defined", " 0"),
    "qb_andefok": ('DECLARE SUB s ()\r\nDIM SHARED c(2)\r\nDIM e(4)\r\ns\r\nSUB s\r\nSTATIC f()\r\nSHARED e()\r\n'
                   'REDIM g(3)\r\nIF 0 THEN h(1) = 1\r\nPRINT UBOUND(c); UBOUND(e); UBOUND(g); UBOUND(h)\r\nEND SUB\r\n',
                   " 2  4  3  10 \r\n", "ERROR"),
    # SINGLE arithmetic stored to a DOUBLE was rounded to 7 digits first: .3333333432674408 for
    # x / 3. BC keeps the x87's precision to the store; constants it folds as SINGLEs, not ^.
    "qb_dblx": ('x = 1: a% = 1: b% = 3: c# = a% / b%: d# = x / 3: e# = 2 ^ .5: PRINT c#; d#; e#\r\n'
                'c# = -1 / 3: d# = -x / 3: e# = (x + 1) / 3: f# = x / 3 + 1: PRINT c#; d#; e#; f#\r\n'
                'x = 2: c# = SQR(x) / 3: d# = 1 / 3 + 1#: PRINT c#; d#\r\n',
                " .3333333333333333  .3333333333333333  1.414213562373095 \r\n"
                "-.3333333432674408 -.3333333333333333  .6666666666666666  1.333333333333333 \r\n"
                " .4714045207910317  1.333333343267441 \r\n", "ERROR"),
    "qb_gosubsc": ('DECLARE SUB s ()\r\ns\r\nEND\r\nm: PRINT "m": RETURN\r\nSUB s\r\nGOSUB m\r\nEND SUB\r\n',
                   "Subprogram error", None),
    # Unhandled: ERROR n's message; RESUME with no error; ON ERROR GOTO 0 in the handler.
    # A full stack was ?OUT OF MEMORY, which ON ERROR trapped; BC's is Out of stack space, not
    # trapped, its line, module and address after.
    "qb_err_stack": ('ON ERROR GOTO h\r\n20 GOSUB 20\r\nh: PRINT "trapped": END\r\n',
                     "Out of stack space in line 20 in module QB_ERR_S at address ", "trapped"),
    # Out of bounds was Illegal function call below 0 or past a word, and an ERASEd dynamic array
    # was made again, 11 elements: QBasic's are all Subscript out of range.
    "qb_subs": ('DIM s(1 TO 3)\r\nREDIM d(2)\r\nON ERROR GOTO h\r\nPRINT s(-1)\r\nx& = 70000: PRINT s(x&)\r\n'
                'ERASE d: PRINT d(0)\r\nEND\r\nh: PRINT ERR: RESUME NEXT\r\n', " 9 \r\n 9 \r\n 9 \r\n", "ERROR"),
    # DIM kept which subscripts had no TO in 24 bits: the 26th's hi and base swapped, 1 TO 0.
    "qb_dim26": ('DIM a(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1)\r\na(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1) = 7\r\nPRINT a(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1)\r\n', " 7 \r\n", "ERROR"),
    "qb_errn": ("ERROR 11\r\n", "Division by zero in line", None),
    "qb_errun": ("ERROR 99\r\n", "Unprintable error in line", None),
    "qb_resnoerr": ("RESUME\r\n", "RESUME without error in line", None),
    "qb_onerr0": ("ON ERROR GOTO h\r\nERROR 6\r\nEND\r\nh: ON ERROR GOTO 0\r\n", "Overflow in line", None),
    # REDIM frees the old array; a SUB's REDIM of its parameter is the caller's. ERASE zeroes a
    # DIM's array, frees a REDIM's.
    "qb_redim": ("DECLARE SUB g (q())\r\nREDIM a(2): a(1) = 5: REDIM a(1 TO 7): PRINT a(1); LBOUND(a); UBOUND(a)\r\n"
                 "g a(): PRINT UBOUND(a); a(9)\r\nDIM s(3): s(2) = 4: ERASE s: PRINT s(2); UBOUND(s)\r\n"
                 "ERASE a: REDIM a(-1 TO 1): PRINT LBOUND(a)\r\n"
                 "SUB g (q())\r\nREDIM q(9): q(9) = 3\r\nEND SUB\r\n",
                 " 0  1  7 \r\n 9  3 \r\n 0  3 \r\n-1 \r\n", "ERROR"),
    # Freeing an array moves the arrays after it: their strings' blocks must follow their Descs,
    # or the next collection drops them.
    "qb_erase_str": ("FOR i = 1 TO 50: c$ = STRING$(100, 66): NEXT\r\n"
                     "REDIM a(500): DIM b$(3): b$(1) = \"x\" + \"y\": ERASE a\r\n"
                     "x = FRE(\"\"): FOR i = 1 TO 50: c$ = STRING$(100, 65): NEXT: PRINT b$(1)\r\n", "xy\r\n", "ERROR"),
    # A procedure's local arrays went with it only at the next RUN: 3000 calls ran out of memory.
    "qb_localarr": ("DECLARE SUB p (n)\r\nFOR i = 1 TO 3000: p i: NEXT: PRINT \"ok\"\r\n"
                    "SUB p (n)\r\nDIM a(2000): a(1) = n\r\nEND SUB\r\n", "ok\r\n", "ERROR"),
    # ERROR through the statement grammar: its CALL's number, past A_BASE, ran as an action instead.
    "qb_error_k": ("ON ERROR GOTO h: ERROR 57: END\r\nh: PRINT ERR: RESUME NEXT\r\n", " 57 \r\n", "Syntax error"),
    # QBasic's RND: its 24-bit generator, the seed 50000h at the start; 0 repeats; RANDOMIZE.
    "qb_rnd": ("PRINT RND; RND(0); RND\r\nRANDOMIZE 1234: PRINT RND\r\n",
               " .7055475  .7055475  .533424 \r\n", "ERROR"),
    # Numbers as their bytes, MBF too; MKI$ is 2 bytes.
    "qb_mki": ("PRINT CVI(MKI$(-5)); CVL(MKL$(70000)); CVS(MKS$(1.5)); CVD(MKD$(2.25#))\r\n"
               "PRINT CVSMBF(MKSMBF$(-3.5)); CVDMBF(MKDMBF$(1.1#)); ASC(MID$(MKSMBF$(1), 4)); LEN(MKI$(1))\r\n"
               "PRINT MKI$(40000)\r\n",
               "-5  70000  1.5  2.25 \r\n-3.5  1.1  129  2 \r\nOverflow in line", None),
    # ENVIRON sets, replaces, removes; DATE$ = and the rest of its line: the routine kept the VM's
    # ip in ESI and lost the line's next statement.
    "qb_environ": ("ENVIRON \"QZ=a\": ENVIRON \"QZ=bc\": PRINT ENVIRON$(\"qz\"); LEN(ENVIRON$(\"QZ\"))\r\n"
                   "ENVIRON \"QZ=\": PRINT \"[\"; ENVIRON$(\"QZ\"); \"]\"\r\n"
                   "DATE$ = \"02-03-2001\": PRINT DATE$: PRINT LEN(TIME$); TIMER > 0; LEN(INKEY$)\r\n",
                   "bc 2 \r\n[]\r\n02-03-2001\r\n 8 -1  0 \r\n", "ERROR"),
    # TRON, TROFF: the editor's trace; on their own, nothing.
    "qb_tron": ("TRON: PRINT 1: TROFF: PRINT 2\r\n", " 1 \r\n 2 \r\n", "ERROR"),
    # DOUBLE: 16 digits, D exponents, 8-byte cells through arrays, READ, FIX/INT.
    "qb_dbl": ("a# = 1# / 3#: PRINT a#; 1D+20; 1.5D-07; 2# ^ .5\r\n"
               "DIM d(3) AS DOUBLE: d(2) = 3.141592653589793#: PRINT d(2) * 2\r\n"
               "READ r#: PRINT r#; FIX(-2.5#); INT(-2.5#); CDBL(1) / 3\r\n"
               "DATA 1.000000000000001\r\n",
               " .3333333333333333  1D+20  .00000015  1.414213562373095 \r\n"
               " 6.283185307179586 \r\n"
               " 1.000000000000001 -2 -3  .3333333333333333 \r\n", "ERROR"),
    # The first digit's power from LOG10 came out one high near 10^n: 10000000 was printed.
    # 20 digits overflowed the 64-bit mantissa: -8.4D+19 was printed.
    "qb_dbl_digits": ("PRINT 9999999.999999994#; 99999999999999.94#\r\n"
                      "PRINT 12345678901234567890#\r\n",
                      " 9999999.999999994  99999999999999.94 \r\n 1.234567890123457D+19 \r\n",
                      "ERROR"),
    # A DOUBLE BYREF, a FUNCTION's DOUBLE result, a record's DOUBLE field.
    "qb_dbl_proc": ("DECLARE SUB tw (x AS DOUBLE)\r\nDECLARE FUNCTION h# (x#)\r\n"
                    "TYPE r\r\na AS INTEGER\r\nd AS DOUBLE\r\nEND TYPE\r\n"
                    "DIM v AS r: v.d = 2.5#: v.a = 7\r\n"
                    "y# = 1.25#: tw y#: PRINT y#; h#(y#); v.d; v.a\r\n"
                    "SUB tw (x AS DOUBLE)\r\nx = x * 2\r\nEND SUB\r\n"
                    "FUNCTION h# (x#)\r\nt# = x# / 4: h# = t# + 1D+15\r\nEND FUNCTION\r\n",
                    " 2.5  1000000000000001  2.5  7 \r\n", "ERROR"),
    # BC's limits: EXP and ^ give infinity up to 2^32767.5, then error 6; SIN past 2^31 error 5.
    # DOSBox's FXAM sees no infinity: 2 ^ 100000 was infinity, not error 6.
    "qb_dbl_range": ("ON ERROR GOTO h\r\nPRINT EXP(22712.6#): PRINT EXP(22712.8#)\r\n"
                     "PRINT 10# ^ 500: PRINT 2# ^ 100000: PRINT SIN(1D+300): PRINT SQR(4#)\r\n"
                     "END\r\nh: PRINT \"e\"; ERR: RESUME NEXT\r\n",
                     " 1.#INF \r\ne 6 \r\n 1.#INF \r\ne 6 \r\ne 5 \r\n 2 \r\n", "ERROR"),
    # Sequential files: PRINT #, WRITE #, LINE INPUT #, INPUT #, EOF past the last CR LF, APPEND.
    # OPEN's FOR OUTPUT, a two-byte keyword inside a statement, was a syntax error.
    "qb_file_seq": ("OPEN \"T1.TXT\" FOR OUTPUT AS #1\r\nPRINT #1, \"hello\"; 42\r\nWRITE #1, \"a,b\", 3.5, -7\r\nCLOSE #1\r\nOPEN \"T1.TXT\" FOR INPUT AS #2\r\nLINE INPUT #2, a$\r\nPRINT \"[\" + a$ + \"]\"\r\nINPUT #2, b$, c, d\r\nPRINT b$; c; d\r\nPRINT EOF(2), LOF(2)\r\nCLOSE\r\nPRINT FREEFILE\r\nOPEN \"T1.TXT\" FOR APPEND AS #1\r\nPRINT #1, \"more\"\r\nCLOSE 1\r\nOPEN \"T1.TXT\" FOR INPUT AS 1\r\nDO WHILE NOT EOF(1)\r\nLINE INPUT #1, x$: PRINT x$\r\nLOOP\r\nCLOSE\r\nKILL \"T1.TXT\"\r\nPRINT \"done\"\r\n",
                    "[hello 42 ]\r\na,b 3.5 -7 \r\n-1             25 \r\n 1 \r\nhello 42 \r\n"
                    "\"a,b\",3.5,-7\r\nmore\r\ndone\r\n", "ERROR"),
    # INPUT$ and SEEK on exact places; INPUT$ gave its error code as the chars read.
    "qb_file_pos": ("f = FREEFILE\r\nOPEN \"Q.DAT\" FOR OUTPUT AS #f\r\nPRINT #f, \"AB\";\r\nCLOSE #f\r\nOPEN \"Q.DAT\" FOR APPEND AS #f\r\nPRINT #f, \"CD\";\r\nCLOSE #f\r\nOPEN \"Q.DAT\" FOR INPUT AS #f\r\nPRINT LOF(f)\r\na$ = INPUT$(2, f)\r\nPRINT a$, SEEK(f)\r\nSEEK #f, 3\r\nPRINT SEEK(f)\r\nb$ = INPUT$(2, f)\r\nPRINT b$, EOF(f)\r\nKILL \"Q.DAT\"\r\n",
                    " 4 \r\nAB             3 \r\n 3 \r\nCD            -1 \r\n", "ERROR"),
    # BINARY and RANDOM: GET, PUT, FIELD, LSET, RSET, LOC, LOF, SEEK. GET and PUT's left-out
    # record pushed nothing.
    "qb_file_rec": ("OPEN \"B.DAT\" FOR BINARY AS #1\r\na% = 258: b& = 65536 + 3: c! = 1.5: d# = 2.25: s$ = \"xyz\"\r\nPUT #1, , a%\r\nPUT #1, , b&\r\nPUT #1, , c!\r\nPUT #1, , d#\r\nPUT #1, , s$\r\nPRINT LOF(1), LOC(1), SEEK(1)\r\nGET #1, 1, x%\r\nGET #1, , y&\r\nGET #1, , z!\r\nGET #1, , w#\r\nt$ = SPACE$(3)\r\nGET #1, , t$\r\nPRINT x%; y&; z!; w#; t$\r\nCLOSE\r\nOPEN \"R.DAT\" FOR RANDOM AS #1 LEN = 10\r\nFIELD #1, 4 AS n$, 6 AS m$\r\nLSET n$ = \"ab\": RSET m$ = \"cd\"\r\nPUT #1, 2\r\nLSET n$ = \"zz\"\r\nGET #1, 2\r\nPRINT \"[\" + n$ + \"][\" + m$ + \"]\", LOC(1), LOF(1)\r\nCLOSE\r\nKILL \"B.DAT\": KILL \"R.DAT\"\r\n",
                    " 21            21            22 \r\n 258  65539  1.5  2.25 xyz\r\n"
                    "[ab  ][    cd]               2             20 \r\n", "ERROR"),
    # File errors as QBasic numbers them; the other OPEN, FILEATTR, LOCK, NAME, the directories,
    # BSAVE and BLOAD.
    "qb_file_misc": ("ON ERROR GOTO h\r\nOPEN \"NOPE.TXT\" FOR INPUT AS #1\r\nOPEN \"O\", #1, \"A.TXT\": PRINT #1, \"x\": OPEN \"A.TXT\" FOR OUTPUT AS #1\r\nPRINT FILEATTR(1, 1); FREEFILE\r\nLOCK #1: UNLOCK #1, 1 TO 2\r\nCLOSE #1, #2\r\nNAME \"A.TXT\" AS \"B.TXT\"\r\nOPEN \"B.TXT\" FOR INPUT AS 3: LINE INPUT #3, l$: PRINT l$: INPUT #3, z$\r\nCLOSE\r\nMKDIR \"QD\": CHDIR \"QD\": CHDIR \"..\": RMDIR \"QD\"\r\nRMDIR \"QD\"\r\nDIM w%(3): w%(0) = 1: w%(1) = 2: w%(3) = 9\r\nBSAVE \"W.BIN\", VARPTR(w%(0)), 16\r\nDIM v%(3): BLOAD \"W.BIN\", VARPTR(v%(0))\r\nPRINT v%(0); v%(1); v%(3)\r\nKILL \"W.BIN\": KILL \"B.TXT\"\r\nPRINT #7, 1\r\nEND\r\nh: PRINT \"e\"; ERR: RESUME NEXT\r\n",
                     "e 53 \r\ne 55 \r\n 2  2 \r\nx\r\ne 62 \r\ne 76 \r\n 1  2  9 \r\ne 52 \r\n",
                     "ERROR"),
    # &H and &O in DATA and VAL, as in the program's text: a syntax error.
    "qb_data_amp": ("READ a%, b%: PRINT a%; b%; VAL(\"&O17\")\r\nDATA &H1234, -2\r\n",
                    " 4660 -2  15 \r\n", "ERROR"),
    # STR$ of a LONG went through a SINGLE: 21096440.
    "qb_str_long": ("x& = 21096439: PRINT STR$(x&); STR$(-5)\r\n", " 21096439-5\r\n", "ERROR"),
    # A carry through 9s looped for ever; one into no digits ran LOOP from ECX 0; a SINGLE was
    # rounded to 7 digits first: 1.01.
    "qb_using_num": ('PRINT USING "##.#"; 9.96; .5\r\nPRINT USING "#"; .5; 9.5\r\n'
                     'PRINT USING "##.##"; 1.005!; 2.675!\r\nPRINT USING "**$###.##"; -5\r\n',
                     "10.0 0.5\r\n1%10\r\n 1.00 2.67\r\n***-$5.00\r\n", "ERROR"),
    "qb_using_str": ('PRINT USING "!|\\  \\|&"; "Hi"; "abcdef"; "x";\r\nPRINT USING "_##"; 5\r\n',
                     "H|abcd|x#5\r\n", "ERROR"),
    # USING beside the action in PRINT's grammar: a two-byte keyword first was a syntax error.
    "qb_print_kw2": ('PRINT STRING$(3, "z"); LCASE$("AB")\r\n', "zzzab\r\n", "ERROR"),
    # A line past 255 bytes wrapped the input buffer: LINE INPUT # gave 45 of 300.
    "qb_line_long": ('OPEN "L.TMP" FOR OUTPUT AS #1: PRINT #1, STRING$(300, "z"): '
                     'PRINT #1, STRING$(290, "y"); ",7": CLOSE\r\n'
                     'OPEN "L.TMP" FOR INPUT AS #1: LINE INPUT #1, a$: INPUT #1, b$, n%: CLOSE\r\n'
                     'KILL "L.TMP": PRINT LEN(a$); LEN(b$); n%\r\n', " 300  290  7 \r\n", "ERROR"),
    # INPUT$ over 255 was an Illegal function call.
    "qb_inps_long": ('OPEN "I.TMP" FOR OUTPUT AS #1: PRINT #1, STRING$(300, "z"); "ab": CLOSE\r\n'
                     'OPEN "I.TMP" FOR INPUT AS #1: a$ = INPUT$(301, #1): b$ = INPUT$(1, 1): CLOSE\r\n'
                     'KILL "I.TMP": PRINT LEN(a$); RIGHT$(a$, 2); b$\r\n', " 301 zab\r\n", "ERROR"),
    # DEF FN had one numeric parameter: several, typed, a string's, EXIT DEF and END DEF were
    # syntax errors. Its parameters local, the rest the module's; arguments by value.
    "qb_deffn_qb": ('DEF FNadd% (a AS INTEGER, b AS INTEGER) = a + b\r\nDEF FNup$ (s$) = UCASE$(s$) + "!"\r\n'
                    'y = 10: DEF FNy (x) = x + y\r\nDEF FNfact# (n%)\r\n'
                    '  IF n% <= 1 THEN FNfact# = 1: EXIT DEF\r\n  FNfact# = n% * FNfact#(n% - 1)\r\nEND DEF\r\n'
                    'x = 7: n% = 3: PRINT FNadd%(2, 5); FNup$("ab"); FNy(1); FNfact#(10); x; n%\r\n',
                    " 7 AB! 11  3628800  7  3 \r\n", "ERROR"),
    # Parameters' AS types were one table by name: the last procedure's n AS DOUBLE made Half's
    # n AS SINGLE another variable; Half(4) gave 0.
    "qb_par_type": ('DECLARE FUNCTION Half! (n AS SINGLE)\r\nDECLARE FUNCTION Dbl# (n AS DOUBLE)\r\n'
                    'PRINT Half(4); Dbl(8)\r\nFUNCTION Half! (n AS SINGLE)\r\n  Half! = n / 2\r\nEND FUNCTION\r\n'
                    'FUNCTION Dbl# (n AS DOUBLE)\r\n  Dbl# = n * 2\r\nEND FUNCTION\r\n', " 2  16 \r\n", "ERROR"),
    # A TYPE's array parameter was a SINGLE array, another SUB's arr() AS LONG a record array
    # (type mismatch); a record not yet used went in as no record, its fields set in a copy.
    "qb_par_rec": ('TYPE C\r\n x AS LONG\r\n y AS LONG\r\nEND TYPE\r\nDIM n(1) AS LONG, p(1) AS C, q AS C\r\n'
                   'CALL FN1(n()): CALL FP(p()): CALL G(q)\r\nPRINT n(1); p(1).y; q.x; q.y\r\n'
                   'SUB FN1 (arr() AS LONG)\r\n  arr(1) = 8\r\nEND SUB\r\n'
                   'SUB FP (arr() AS C)\r\n  arr(1).y = 4\r\nEND SUB\r\n'
                   'SUB G (r AS C)\r\n  r.x = 5: r.y = r.x + 1\r\nEND SUB\r\n', " 8  4  5  6 \r\n", "ERROR"),
    # LEN of a number was a type mismatch.
    "qb_len_num": ('DIM d AS DOUBLE, i AS INTEGER\r\ns$ = "abc"\r\n'
                   'PRINT LEN(a%); LEN(b&); LEN(c!); LEN(e#); LEN(d); LEN(i); LEN(s$)\r\n',
                   " 2  4  4  8  8  2  3 \r\n", "ERROR"),
    # & after a whole number was a syntax error.
    "qb_lit_long": ("PRINT 70000&; -5%\r\n", " 70000 -5 \r\n", "ERROR"),
    # SINGLE EXP overflowed at SINGLE's range: 22712 was error 6, not BC's infinity.
    "qb_exp_sng": ('ON ERROR GOTO h\r\nPRINT "a"\r\ns! = 22712: z# = EXP(s!): PRINT "ok"\r\n'
                   's! = 22713: z# = EXP(s!): PRINT "ok"\r\nEND\r\nh: PRINT ERR: RESUME NEXT\r\n',
                   "a\r\nok\r\n 6 \r\nok\r\n", "ERROR"),
    # SINGLE SIN past 2^31 gave 0 or a value, not error 5.
    "qb_trig_sng": ('ON ERROR GOTO h\r\ns! = 1E13: x = SIN(s!): x = COS(s!): x = TAN(s!)\r\n'
                    'END\r\nh: PRINT ERR: RESUME NEXT\r\n', " 5 \r\n 5 \r\n 5 \r\n", "ERROR"),
    # SINGLE ^ gave 0, 1, 0 and error 6 where BC's gives error 5: 0 ^ -1, a negative base to a
    # power past 2^31.
    "qb_pow_sng": ('ON ERROR GOTO h\r\nz! = 0: m! = -1: t! = -2: x = z! ^ m!: x = m! ^ 2147483648!\r\n'
                   'x = z! ^ -2147483649!: x = t! ^ 1E+30\r\nEND\r\nh: PRINT ERR: RESUME NEXT\r\n',
                   " 5 \r\n 5 \r\n 5 \r\n 5 \r\n", "ERROR"),
    # SINGLE's own reader stopped at D, a lowercase e, a space before E: 1.5 -.5 1 .5.
    "qb_val_exp": ('PRINT VAL(" 1.5D3"); VAL("-.5e-3"); VAL("1 e2"); VAL(".5d1")\r\n',
                   " 1500 -.0005  100  5 \r\n", "ERROR"),
    # BC's SINGLE: past its range infinity, under it a denormal; VAL a DOUBLE. VAL gave 1.234568,
    # a denormal 0 (and 1 / it Division by zero), the rest error 6.
    # A compile error gave BC's line alone; now the line, a mark under the place, what is wrong
    # and what to do.
    "qb_diag_end": ("PRINT a(1\r\n", "  print a(1\r\n           ^\r\nThe PRINT statement is incomplete: "
                    "the line ends where more is needed.\r\nPRINT is written:\r\n  PRINT [#file,]", None),
    "qb_diag_goto": ("ON 1 PRINT\r\n", "\r\n       ^\r\n`print` should be followed by GOTO or GOSUB "
                     "here.\r\nWrite `ON expression GOTO label, label`.", None),
    "qb_diag_tm": ('x = "a" + 1\r\n', '\r\n            ^\r\n`1` has the wrong type for this place (', None),
    # A statement the automaton rejected said only that it does not fit; now what it wanted.
    "qb_diag_exp": ("FOR i = 1 3\r\nNEXT\r\n", "Expected `TO` in the FOR statement, but found `3`.\r\n"
                    "FOR is written:\r\n  FOR counter = start TO end [STEP step]", None),
    "qb_diag_exp2": ('OPEN "x" FOR FOO AS 1\r\n', "Expected `INPUT`, `APPEND`, `BINARY`, `OUTPUT` or "
                     "`RANDOM` in the OPEN statement, but found `FOO`.", None),
    "qb_diag_kw": ('NAME "a" "b"\r\n', 'Expected `AS` in the NAME statement, but found `"b"`.', None),
    # The blocks' errors were a Syntax error, or NEXT without FOR, with no line shown and no words.
    "qb_blk_open": ("PRINT 1\r\nFOR i = 0 TO 9\r\nPRINT i\r\n", ": FOR without NEXT\r\n", None),
    "qb_blk_open2": ("PRINT 1\r\nFOR i = 0 TO 9\r\nPRINT i\r\n", "  for i = 0 to 9\r\n  ^\r\nThis FOR loop,"
                     " counting with `i`, is never closed", None),
    "qb_blk_nextv": ("FOR i = 1 TO 3\r\nNEXT j\r\n", "  next j\r\n       ^\r\nThis NEXT ends a loop counting "
                     "with `j`, but the loop open here counts with `i`.\r\nWrite `NEXT i`, or just NEXT.", " 1"),
    "qb_blk_cross": ("FOR i = 1 TO 3\r\nDO\r\nNEXT i\r\n", ": NEXT without FOR\r\n"
                     "", None),
    "qb_blk_cross2": ("FOR i = 1 TO 3\r\nDO\r\nNEXT i\r\n", "the DO block started inside it is still open",
                      None),
    "qb_blk_loop": ("PRINT 1\r\nLOOP\r\n", ": LOOP without DO\r\n", " 1"),
    # A FOR's limit and step are the procedure's locals: a recursive call's loop keeps its own.
    "qb_for_rec": ("DECLARE SUB r (n%)\r\nr 3\r\nSUB r (n%)\r\nFOR i% = 1 TO n%\r\n"
                   "IF i% = 1 AND n% > 1 THEN r n% - 1\r\nPRINT n%;\r\nNEXT\r\nEND SUB\r\n",
                   " 1  2  2  3  3  3 ", "ERROR"),
    # NEXT j, i closes both; a FOR ending its line loops to the next; EXIT FOR leaves at once.
    "qb_for_next2": ("FOR i = 1 TO 2\r\nFOR j = 1 TO 2: PRINT i * 10 + j;: NEXT j, i\r\n"
                     "FOR k = 1 TO 9: IF k = 3 THEN EXIT FOR\r\nNEXT: PRINT k\r\n",
                     " 11  12  21  22  3 \r\n", "ERROR"),
    # An undefined label was found when its GOTO ran, after the program's output: now at RUN.
    "qb_lbl_goto": ("PRINT 1\r\nIF 0 THEN GOTO nowhere\r\n", "There is no label `nowhere`", " 1"),
    "qb_lbl_on": ("PRINT 1\r\nON 2 GOSUB a, b\r\nEND\r\na: RETURN\r\n", "There is no label `b`", " 1"),
    "qb_lbl_restore": ("PRINT 1\r\nIF 0 THEN RESTORE nowhere\r\nDATA 1\r\n", "There is no label `nowhere`", " 1"),
    "qb_lbl_run": ("PRINT 1\r\nIF 0 THEN RUN 90\r\n", "Label not defined", " 1"),
    "qb_lbl_resume": ("ON ERROR GOTO h\r\nPRINT 1\r\nEND\r\nh: RESUME 90\r\n", ": Label not defined\r\n", " 1"),
    # &H and &O took no type suffix: &HFF& was a Syntax error at the &.
    "qb_hex_sfx": ("CONST a% = &H10%, b& = &HFF&: PRINT a%; b&; &O17&; &HFFFF&\r\n", " 16  255  15  65535 \r\n", "ERROR"),
    # A call of a SUB or FUNCTION only DECLAREd was a syntax error; as BC's, it is rejected before the run,
    # not trapped.
    "qb_pdecl": ('DECLARE SUB ext (a%, b$)\r\nDECLARE FUNCTION fx% (a%)\r\nON ERROR GOTO h\r\next 1, "x"\r\n'
                 'CALL ext(2, "y")\r\ny = fx%(3) + 1\r\nPRINT "d"\r\nEND\r\nh: PRINT ERR;: RESUME NEXT\r\n',
                 ": Subprogram not defined\r\n", " 35"),
    # A line's text was 255 characters at most, its body 2047 bytes, positions 11 bits: Out of memory at load.
    "qb_long_line": ("FOR i = 1 TO 3: " + "x = x + 1: " * 300 + "NEXT: IF x > 0 THEN PRINT x ELSE PRINT -1\r\n"
                     "DO: k = k + 1: " + "y = y + 1: " * 250 + "LOOP UNTIL k = 2: PRINT y\r\n", " 900 \r\n 500 \r\n",
                     "ERROR"),
    # The program store was 64 KB: 6000 short lines were Out of memory.
    "qb_big_store": ("X = 0\r\n" + "X = X + 1\r\n" * 6000 + "PRINT X\r\n", " 6000 \r\n", "memory"),
    # A '_' after a blank ended no line: Syntax error. Not one in a string or a comment.
    "qb_cont": ('A = 1 + _\r\n    2\r\nPRINT A; _\r\n  "x_ _"\r\nPRINT "a _"\r\nPRINT 3 \' t _\r\n'
                'PRINT 4 REM m _\r\nPRINT 5\r\n', " 3 x_ _\r\na _\r\n 3 \r\n 4 \r\n 5 \r\n", "rror"),
    # A line past the 32 KB input buffer: Out of memory.
    "qb_phys_line": ("X = 0: " + "X = X + 1: " * 3500 + "PRINT X\r\n", " 3500 \r\n", "memory"),
    "qb_long_err": ("z = 1: " * 60 + "PRINT (z\r\n", "\r\n" + " " * 430 + "^\r\nThe PRINT statement is incomplete", None),
    # A file's errors named no line of it ("in line No line number"): each one bisected by hand.
    # The line read from the file, a continued one's first; blank lines count.
    "qb_where_compile": ("PRINT 1\r\n\r\nX = 1 + _\r\n  (\r\nPRINT 3\r\n", ".BAS:3: Syntax error\r\n  X = 1 + (\r\n", " 1"),
    "qb_where_dupdef": ("CONST SP = 1\r\nPRINT 1\r\nsp = 2\r\n", ".BAS:3: Duplicate definition\r\n  SP = 2\r\n  ^\r\n`SP` is", " 1"),
    # An include's include was looked for in the current folder, not beside it: File not found.
    "include_beside": ("REM $INCLUDE: 'SUB/A.BI'\r\nPRINT 3\r\n", " 1 \r\n 2 \r\n 3 \r\n", "ERROR"),
    "qb_where_include": ("PRINT 1\r\n\r\nREM $INCLUDE: 'WHERE.BI'\r\nPRINT 2\r\n", "WHERE.BI:3: Syntax error\r\n  Y = 1 +\r\n", " 1"),
    # Errors loading the file (a label twice, a number past 63999): the bare message, no line.
    "qb_where_label": ("PRINT 1\r\nfoo:\r\nfoo:\r\n", ".BAS:3: Duplicate label\r\n  foo:\r\n  ^\r\n", " 1"),
    "qb_where_lnum": ("PRINT 1\r\n70000 PRINT 2\r\n", ".BAS:2: Syntax error\r\n  70000 PRINT 2\r\n  ^\r\n", " 1"),
    # A keyword for a procedure's name: Syntax error, with neither the line nor a word why.
    "qb_where_reserved": ("PRINT 1\r\nFUNCTION Field% (x AS INTEGER)\r\nEND FUNCTION\r\n",
                          ".BAS:2: Syntax error\r\n  function field %(x as integer)\r\n           ^\r\n`field` is a reserved word", " 1"),
    "qb_blk_next2": ("FOR i = 1 TO 2: FOR j = 1 TO 2: NEXT j, i: PRINT \"ok\"\r\n", "ok", "ERROR"),
    "qb_sng_range": ('PRINT VAL("1.23456789012"); 1E-45; 1 / 1E-45\r\n'
                     'a! = 3.4E+38: d# = 1D+40: x! = 1E-38: PRINT a! * 10; CSNG(d#); x! / 1000\r\n',
                     " 1.23456789012  1.401298E-45  1.#INF \r\n 1.#INF  1.#INF  9.999666E-42 \r\n",
                     "ERROR"),
}

# Run as files with 8 MB: the heap is the memory free.
BIG = {
    # VARPTR's cell was a literal from the compile: the cache grown past 64 KB while linking moved
    # the cells, and line 1's POKE missed a% (" 1  9").
    "qb_varptr_link": ("a% = 1: POKE VARPTR(a%), 7: PRINT a%;\r\n"
                       + "".join("b = 1" + "".join(f": b = b * 3 + {k}" for k in range(20)) + "\r\n" for _ in range(600))
                       + "POKE VARPTR(a%), 9: PRINT a%\r\n", " 7  9 \r\n", "ERROR"),
    # The heap was a fixed 4 MB whatever the machine had: 4.8 MB of arrays were Out of memory.
    "qb_heap": ('DIM a(999, 99) AS DOUBLE, b(999, 99) AS DOUBLE, c(999, 99) AS DOUBLE\r\n'
                'DIM d(999, 99) AS DOUBLE, e(999, 99) AS DOUBLE, f(999, 99) AS DOUBLE\r\n'
                'f(999, 99) = 3: PRINT f(999, 99)\r\n', " 3 \r\n", "ERROR"),
    # Names and variables were fixed tables of 4,096: the 4,097th was Out of memory.
    # Made while a string was in the heap the table could not grow: Out of memory; failing, it crashed.
    "many_vars_heap": ('a$ = "s" + STR$(1)\r\n' + "".join(f"v{i} = {i}\r\n" for i in range(1500)) + "PRINT a$; v1400\r\n",
                       "s 1 1400 \r\n", "ERROR"),
    # The code cache grown in a link moved the program store up: every literal compiled before
    # pointed at its text's old place. 1777 lines of a = 1 printed bytes of code for "start".
    "big_code_lits": ('PRINT "start"\r\n' + "a = 1\r\n" * 1777 + 'PRINT "end"\r\n', "start\r\nend\r\n", "ERROR"),
    "big_code_lits_far": ("".join(f'PRINT "lit{i}"\r\n' for i in range(400)) + "a0 = a0 + 1 * 2 - b0 / 3\r\n" * 3000
                          + 'PRINT "end"\r\n', "lit0\r\nlit1\r\n", "ERROR"),
    # A procedure named by a keyword (FIELD) was a Syntax error with no line or caret.
    "proc_keyword_name": ("PRINT 1\r\nFUNCTION Field% (x AS INTEGER)\r\nEND FUNCTION\r\n",
                          "function field %(x as integer)\r\n           ^", None),
    "many_vars": ("".join(f"v{i} = {i}\r\n" for i in range(5000)) + "PRINT v0; v4999\r\n", " 0  4999 \r\n", "ERROR"),
}

# Run as files; what standard error showed. A file's CR LF counted as two lines: line 3, 5.
WARN = {
    "warn_seg": ("PRINT 1\r\nDEF SEG = VARSEG(x)\r\nDEF SEG: PRINT SSEG(a$)\r\n",
                 "Warning: line 2: DEF SEG has no effect in 32-bit mode\n"
                 "Warning: line 2: VARSEG has no effect in 32-bit mode\n"
                 "Warning: line 3: DEF SEG has no effect in 32-bit mode\n"
                 "Warning: line 3: SSEG has no effect in 32-bit mode"),
    # LF and CR LF, mixed: one line each, blank ones too.
    "warn_lf": ("PRINT 1\nDEF SEG\r\n\n\r\nDEF SEG\nDEF SEG\r\n",
                "Warning: line 2: DEF SEG has no effect in 32-bit mode\n"
                "Warning: line 5: DEF SEG has no effect in 32-bit mode\n"
                "Warning: line 6: DEF SEG"),
    # DIM a(), ours, and nothing gives the array elements: a REDIM, a() = .., a call passing it, a
    # SUB's SHARED, COMMON. An element's use is a runtime error, found at the line.
    "warn_empty_array": ("type Vec\r\n x as single\r\nend type\r\ncommon shared h() as long\r\n"
                         "declare sub show (x() as long)\r\ndeclare sub fill ()\r\n"
                         "dim a() as long, b() as long, c() as long, d() as long, e() as long, h() as long\r\n"
                         "dim r() as Vec, q() as Vec: redim r(2): q() = r()\r\n"
                         "redim b(2): c() = b(): show d(): fill\r\ndim f() as string\r\nprint 1\r\n"
                         "sub show (x() as long)\r\nend sub\r\nsub fill\r\nshared e()\r\nredim e(3)\r\n"
                         "dim g() as integer\r\nend sub\r\n",
                         "Warning: line 7: nothing REDIMs, assigns or passes a()\n"
                         "Warning: line 10: nothing REDIMs, assigns or passes f()\n"
                         "Warning: line 17: nothing REDIMs, assigns or passes g()"),
}

# A line's code past its room left an op cut short before the error's: linking it never ended,
# or the run stopped with no word. Each line's code is cut at another place. 4800 statements: past
# the room fused too (comp/fuse.c), each X=X one op.
CASES |= {f"qb_code_full{k}": ("Y% = 0: " * k + "X = 0: " + "X=X:" * 4800 + "PRINT X\r\n", "Out of memory", None)
          for k in range(8)}

def bas(*lines: str) -> str:
    return "".join(line + "\r\n" for line in lines)


OV = 'h: PRINT "ov"; ERR: RESUME NEXT'
# BYTE, ours, was a name: an unsigned 8-bit INTEGER, 0 to 255, else Overflow; 1 byte in a record
# and a file, an INTEGER's cell elsewhere; its arithmetic an INTEGER's.
CASES |= {
    "byte_scalar": (bas("DIM b AS BYTE, c AS BYTE", "ON ERROR GOTO h",
                        'b = 255: c = 2.5: PRINT b; c; LEN(b); b + c; STR$(b); VAL("7") + b',
                        "c = 3.5: PRINT c; -c; b * 2", "b = 256", "b = -1", "c = 255.5", "PRINT b; c",
                        "SWAP b, c: PRINT b; c", "END", OV),
                    " 255  2  1  257  255 262 \r\n 4 -4  510 \r\nov 6 \r\nov 6 \r\nov 6 \r\n 255  4 \r\n 4  255 \r\n",
                    "ERROR"),
    "byte_static": (bas("DECLARE SUB s ()", "DIM SHARED g AS BYTE", "g = 10: s: s: PRINT g: s",
                        "SUB s", "STATIC n AS BYTE", "n = n + 100: g = g + 1: PRINT n;", "END SUB"),
                    " 100  200  12 \r\nOverflow", None),
    "byte_proc": (bas("DEFBYTE F", "DECLARE SUB inc (x AS BYTE)", "DECLARE SUB v (BYVAL x AS BYTE)",
                      "DECLARE FUNCTION f (x AS BYTE)", "DIM b AS BYTE, a(3) AS BYTE", "ON ERROR GOTO h",
                      "PRINT f(200)", "b = 254: inc b: a(2) = 9: inc a(2): PRINT b; f(a(2))", "inc b", "PRINT b",
                      "v 300", "v 7", "END", OV,
                      "SUB inc (x AS BYTE)", "x = x + 1", "END SUB",
                      "SUB v (BYVAL x AS BYTE)", 'PRINT "v"; x', "END SUB",
                      "FUNCTION f (x AS BYTE)", "f = x * 2", "END FUNCTION"),
                  "ov 6 \r\n 0 \r\n 255  20 \r\nov 6 \r\n 255 \r\nov 6 \r\nv 7 \r\n", "ERROR"),
    "byte_ptm": (bas("DECLARE SUB s (x AS SINGLE)", "DIM b AS BYTE", "s b", "SUB s (x AS SINGLE)", 'PRINT "ran"',
                     "END SUB"), "Parameter type mismatch", "ran"),
    "byte_arr": (bas("DECLARE SUB fill (a() AS BYTE)", "DIM a(1 TO 3, 2) AS BYTE", "ON ERROR GOTO h",
                     "PRINT LBOUND(a); UBOUND(a); LBOUND(a, 2); UBOUND(a, 2)", "fill a()",
                     "PRINT a(1, 0); a(3, 2); a(2, 1) + a(3, 2)", "a(1, 1) = 256",
                     "REDIM d(5) AS BYTE", "d(5) = 77: PRINT d(5); UBOUND(d)", "ERASE d",
                     "REDIM d(2) AS BYTE: PRINT d(2); UBOUND(d)", "ERASE a: PRINT a(3, 2)", "END", OV,
                     "SUB fill (a() AS BYTE)",
                     "FOR i = LBOUND(a) TO UBOUND(a): FOR j = 0 TO 2: a(i, j) = i * 10 + j: NEXT: NEXT",
                     "END SUB"),
                 " 1  3  0  2 \r\n 10  32  53 \r\nov 6 \r\n 77  5 \r\n 0  2 \r\n 0 \r\n", "ERROR"),
    "byte_rec": (bas("TYPE P", "c AS BYTE", "x AS INTEGER", "d AS BYTE", "END TYPE",
                     "DIM r AS P, q AS P, t(2) AS P", "ON ERROR GOTO h",
                     "r.c = 200: r.x = -1: r.d = 7: t(1).d = 255",
                     "PRINT LEN(r); LEN(r.c); PEEK(VARPTR(r)); PEEK(VARPTR(r) + 5); VARPTR(r.d) - VARPTR(r); "
                     "VARPTR(t(2)) - VARPTR(t(1))",
                     "r.d = 256", "PRINT r.c; r.x; r.d; t(1).d + 1",
                     'OPEN "BYTE.DAT" FOR RANDOM AS #1 LEN = LEN(r)',
                     "PUT #1, 2, r: GET #1, 2, q: PRINT LOF(1); q.c; q.x; q.d", "CLOSE #1", 'KILL "BYTE.DAT"',
                     "END", OV),
                 " 6  1  200  7  5  6 \r\nov 6 \r\n 200 -1  7  256 \r\n 12  200 -1  7 \r\n", "ERROR"),
    "byte_binary": (bas("TYPE P", "c AS BYTE", "d AS BYTE", "END TYPE", "DIM b AS BYTE, r AS P",
                        "b = 65: r.c = 66", 'OPEN "BYTE.BIN" FOR BINARY AS #1',
                        "PUT #1, , b: PUT #1, , r.c: PRINT LOF(1)",
                        "b = 0: GET #1, 1, b: GET #1, 2, r.d: PRINT b; r.d", "CLOSE #1",
                        'OPEN "BYTE.BIN" FOR INPUT AS #1: LINE INPUT #1, s$: PRINT s$: CLOSE #1',
                        'KILL "BYTE.BIN"'),
                    " 2 \r\n 65  66 \r\nAB\r\n", "ERROR"),
    "byte_def": (bas("DEFBYTE B-C", "b = 255: PRINT b; LEN(b)", "c = b + 1"), " 255  1 \r\nOverflow", None),
    "byte_cbyte": (bas("PRINT CBYTE(2.5); CBYTE(254.6); CBYTE(-.4); CBYTE(3) + 255", "PRINT CBYTE(256)"),
                   " 2  255  0  258 \r\nOverflow", None),
    "byte_input": (bas("DIM b AS BYTE", "ON ERROR GOTO h",
                       'OPEN "BYTE.TXT" FOR OUTPUT AS #1: PRINT #1, 200: PRINT #1, 300: CLOSE #1',
                       'OPEN "BYTE.TXT" FOR INPUT AS #1: INPUT #1, b: PRINT b: INPUT #1, b: PRINT b: CLOSE #1',
                       'KILL "BYTE.TXT"', "READ b: PRINT b: READ b: PRINT b", "END", "DATA 17, 999", OV),
                   " 200 \r\nov 6 \r\n 200 \r\n 17 \r\nov 6 \r\n 17 \r\n", "ERROR"),
    "byte_for": (bas("DIM b AS BYTE", "FOR b = 253 TO 255: PRINT b;: NEXT"), " 253  254  255 \r\nOverflow", None),
    "byte_common": (bas("COMMON k AS BYTE, a() AS BYTE", "DIM a(2) AS BYTE", "k = 250: a(2) = 9",
                        'CHAIN "QB_CH4"'), " 250  1  9 \r\nOverflow", None),
    "byte_varptrs": (bas("TYPE P", "c AS BYTE", "d AS BYTE", "END TYPE", "DIM b AS BYTE, r AS P",
                         "b = 64: r.c = 3: r.d = 255",
                         "PRINT ASC(VARPTR$(b)); ASC(VARPTR$(r.c))",
                         'PLAY "MBL=" + VARPTR$(b) + "O=" + VARPTR$(r.c) + "P4"', 'PRINT "ok"'),
                     " 2  1 \r\nok\r\n", "ERROR"),
}

# A BYTE or SHORT array's element was an INTEGER's 4 bytes: 24 by 128 by 128 SHORTs, 1.5 MB, were
# Out of memory, as was any array past a 1 MB cap the 16 MB heap did not need.
CASES |= {
    "narrow_arr_size": (bas("DIM a(1 TO 4) AS BYTE, s(3) AS SHORT, i(1) AS INTEGER",
                            "a(2) = 255: s(1) = -2: s(2) = 300",
                            "PRINT VARPTR(a(2)) - VARPTR(a(1)); VARPTR(s(1)) - VARPTR(s(0)); "
                            "VARPTR(i(1)) - VARPTR(i(0))",
                            "PRINT a(1); a(2); a(3); s(0); s(1); s(2); s(3); PEEK(VARPTR(s(1)))"),
                        " 1  2  4 \r\n 0  255  0  0 -2  300  0  254 \r\n", "ERROR"),
    "narrow_arr_big": (bas("DIM f(23, 127, 127) AS SHORT, g(399999) AS INTEGER",
                           "f(23, 127, 127) = -5: g(399999) = 7: PRINT f(23, 127, 127); g(399999)"),
                       "-5  7 \r\n", "ERROR"),
    "narrow_arr_io": (bas("DIM s(3) AS SHORT, b(3) AS BYTE", "READ s(3), b(1): SWAP s(3), s(2): s(1) = -7",
                          'OPEN "N.DAT" FOR BINARY AS #1: PUT #1, , s(1): PUT #1, , b(1): GET #1, 1, s(0)',
                          "PRINT LOF(1); s(0); s(1); s(2); s(3); b(1)", 'CLOSE #1: KILL "N.DAT"', "DATA -300, 200"),
                      " 3 -7 -7 -300  0  200 \r\n", "ERROR"),
    # Passed by reference, a narrow element is read and written at its width; its neighbours keep theirs.
    "narrow_arr_byref": (bas("DECLARE SUB inc (x AS BYTE)", "DECLARE SUB neg (x AS SHORT)",
                             "DECLARE SUB outer (x AS SHORT)",
                             "DIM a(3) AS BYTE, s(3) AS SHORT, t AS SHORT", "ON ERROR GOTO h",
                             "a(1) = 7: a(2) = 9: a(3) = 7: inc a(2): PRINT a(1); a(2); a(3)",
                             "s(0) = 1: s(1) = 5: s(2) = 1: neg s(1): PRINT s(0); s(1); s(2)",
                             "outer s(1): PRINT s(1); s(2)", "t = 5: outer t: PRINT t; t + 1",
                             "a(2) = 255: inc a(2)", "PRINT a(2); a(3)", "END", OV,
                             "SUB inc (x AS BYTE)", "x = x + 1", "END SUB",
                             "SUB neg (x AS SHORT)", "x = -x", "END SUB",
                             "SUB outer (x AS SHORT)", "neg x: x = x - 1", "END SUB"),
                         " 7  10  7 \r\n 1 -5  1 \r\n 4  1 \r\n-6 -5 \r\nov 6 \r\n 255  7 \r\n", "ERROR"),
}

# NEXT a, b skipped b's NEXT when a's loop was left, by EXIT FOR or never entered: b ran once
# (" 1 done").
CASES |= {
    "qb_next_ab_left": (bas("FOR b = 1 TO 3: PRINT b;: FOR a = 1 TO 0: NEXT a, b", 'PRINT "done"',
                            "FOR b = 1 TO 3", " PRINT b;", " FOR a = 1 TO 5", "  IF a = 2 THEN EXIT FOR",
                            "NEXT a, b", 'PRINT "done"; a; b'),
                        " 1  2  3 done\r\n 1  2  3 done 2  4 \r\n", "ERROR"),
}

# CONTINUE FOR | DO | WHILE, EXIT WHILE and a FOR's counter after EXIT FOR or CONTINUE FOR, ours,
# were Syntax errors. CONTINUE stays a name unless a loop word follows it at a statement's start.
CASES |= {
    "loop_continue": (bas("FOR i = 1 TO 5", " IF i = 2 THEN CONTINUE FOR", " PRINT i;", "NEXT", "PRINT",
                          "DO", " n = n + 1", " IF n MOD 2 THEN n = n + 0: CONTINUE DO", " PRINT n;",
                          "LOOP UNTIL n >= 6", "PRINT",
                          "WHILE w < 6", " w = w + 1", " IF w <> 3 THEN PRINT w; ELSE CONTINUE WHILE",
                          ' PRINT "-";', "WEND", "PRINT"),
                      " 1  3  4  5 \r\n 2  4  6 \r\n 1 - 2 - 4 - 5 - 6 -\r\n", "ERROR"),
    "loop_exit_while": (bas("WHILE 1", " n = n + 1", " IF n = 3 THEN EXIT WHILE", "WEND",
                            "WHILE 1", " FOR i = 1 TO 9", "  IF i = 4 THEN EXIT WHILE", " NEXT",
                            ' PRINT "never"', "WEND", "PRINT n; i"),
                        " 3  4 \r\n", "ERROR"),
    "loop_counter": (bas("FOR y = 1 TO 3", " FOR x = 1 TO 3", "  IF x = 2 THEN EXIT FOR y", "  PRINT y; x;",
                         " NEXT", "NEXT", 'PRINT "|"; y; x',
                         "FOR y = 1 TO 3", " FOR x = 1 TO 3", "  IF x = 2 THEN CONTINUE FOR y",
                         "  PRINT y; x;", " NEXT", ' PRINT "never"', "NEXT", 'PRINT "|"; y; x',
                         "FOR b = 1 TO 3", " FOR a = 1 TO 3", "  IF a = 2 THEN CONTINUE FOR b",
                         "  PRINT b; a;", "NEXT a, b", 'PRINT "|"; a; b'),
                     " 1  1 | 1  2 \r\n 1  1  2  1  3  1 | 4  2 \r\n 1  1  2  1  3  1 | 2  4 \r\n", "ERROR"),
    "loop_continue_name": (bas("continue = 5", "PRINT continue", "GOTO continue", "PRINT 1",
                               "continue:", "PRINT 2"), " 5 \r\n 2 \r\n", " 1"),
    "loop_continue_open": (bas("DEFSTR C", 'continue = "C.TMP"', "OPEN continue FOR OUTPUT AS #1",
                               'PRINT #1, "a"', "CLOSE", "KILL continue", 'PRINT "ok"'), "ok", "ERROR"),
    "loop_continue_none": (bas("PRINT 1", "CONTINUE FOR"),
                           ".BAS:2: CONTINUE not within matching block\r\n  continue for\r\n  ^\r\n"
                           "This CONTINUE FOR is not inside a FOR loop.", " 1"),
    "loop_exit_counter": (bas("FOR i = 1 TO 2", " EXIT FOR q", "NEXT"),
                          ".BAS:2: EXIT not within matching block\r\n  exit for q\r\n           ^\r\n"
                          "No FOR loop counting with `q` is open here.", " 1"),
}

# '_' in a name, ours: my_var was Syntax error, do_it the keyword DO; my_rem's REM a comment, its
# line's continuation lost. A '_' after a blank at a line's end still continues it.
CASES |= {
    "underscore_names": (bas("DECLARE SUB show_it (a_b AS INTEGER)", "TYPE pt_t", "  x_pos AS INTEGER", "END TYPE",
                             "CONST max_n = 4", "DIM p AS pt_t, arr_a(max_n)", "DEF fn_sq (x) = x * x",
                             "p.x_pos = 7: arr_a(max_n) = 9: end_x = 1: x_ = 3", "my_rem = 1 + _", "  2",
                             "show_it p.x_pos: GOSUB do_it", "PRINT arr_a(4); fn_sq(3); end_x; x_; my_rem", "END",
                             'do_it: PRINT "s": RETURN', "SUB show_it (a_b AS INTEGER)", "PRINT a_b", "END SUB"),
                         " 7 \r\ns\r\n 9  9  1  3  3 \r\n", "ERROR"),
}

# FUNCTION f (..) AS type, ours, was a Syntax error, and DECLARE's AS ignored: f was a SINGLE. A
# record result goes to a record of the caller's, one per call, emptied as the FUNCTION starts.
VEC = ("type Vec", " x as single", " y as single", "end type")
CASES |= {
    "func_as_scalar": (bas("declare function half (x) as integer", "declare function greet (n$) as string",
                           "declare function third (x) as double", "declare function big (x) as long",
                           'print half(5); half(7); greet("bo"); third(1); big(3)',
                           "function half (x) as integer", "half = x / 2", "end function",
                           "function greet (n$) as string", 'greet = "hi " + n$', "end function",
                           "function third (x) as double", "third = x / 3#", "end function",
                           "function big (x) as long static", "big = x * 100000", "end function"),
                       " 2  4 hi bo .3333333333333333  300000 \r\n", "ERROR"),
    # The definition's AS, or its none, is the type, whatever a DECLARE says.
    "func_as_declare": (bas("declare function f (x) as integer", "print f(5)", "function f (x)", "f = x / 2",
                            "end function"), " 2.5 \r\n", "ERROR"),
    "func_as_record": (bas(*VEC, "declare function Add (a as Vec, b as Vec) as Vec",
                           "declare function Mk (x!, y!) as Vec", "declare function Swp (v as Vec) as Vec",
                           "declare function Origin () as Vec", "declare function Tri (n%) as Vec",
                           "declare sub Show (v as Vec)", "dim a as Vec, b as Vec, arr(3) as Vec",
                           "a = Mk(1, 2): b = Mk(10, 20)", "Show Add(a, b)", "a = Swp(a): Show a",
                           "Show Add(Add(a, b), Mk(100, 100))", "arr(2) = Mk(7, 8): Show arr(2)",
                           "print Add(a, b).x; Mk(3, 4).y", 'if Mk(5, 0).x > 4 then print "big"',
                           "for i = 1 to 3: arr(i) = Mk(i, i * i): next", "print arr(1).y; arr(2).y; arr(3).y",
                           "Show Origin", "Show Tri(4)", "end",
                           "sub Show (v as Vec)", " print v.x; v.y", "end sub",
                           "function Add (a as Vec, b as Vec) as Vec", " Add.x = a.x + b.x", " Add.y = a.y + b.y",
                           "end function", "function Mk (x!, y!) as Vec", " Mk.x = x!: Mk.y = y!", "end function",
                           "function Swp (v as Vec) as Vec", " Swp.x = v.y: Swp.y = v.x", "end function",
                           "function Origin () as Vec", "end function",
                           "function Tri (n%) as Vec", " if n% = 0 then exit function", " Tri = Tri(n% - 1)",
                           " Tri.x = Tri.x + n%: Tri.y = Tri.y + 1", "end function"),
                       " 11  22 \r\n 2  1 \r\n 112  121 \r\n 7  8 \r\n 12  4 \r\nbig\r\n 1  4  9 \r\n 0  0 \r\n"
                       " 10  4 \r\n", "ERROR"),
    # Each call's result starts empty: a field the FUNCTION leaves is 0, not the last call's.
    "func_as_record_empty": (bas(*VEC, "dim r as Vec", "for i% = 1 to 3: r = Part(i%): print r.x; r.y;: next",
                                 "end", "function Part (i%) as Vec", " if i% = 1 then Part.y = 9", " Part.x = i%",
                                 "end function"), " 1  9  2  0  3  0 ", "ERROR"),
    "func_as_record_whole": (bas(*VEC, "print Mk(2)", "function Mk (x!) as Vec", "end function"),
                             ".BAS:5: Type mismatch\r\n  print Mk(2)\r\n            ^", None),
    "func_as_no_type": (bas("function Mk (x!) as Nope", "end function"),
                        ".BAS:1: Syntax error\r\n  function Mk (x!) as Nope\r\n                      ^", None),
    "func_as_suffix": (bas("print h%(5)", "function h% (x) as integer", "end function"),
                       ".BAS:2: Syntax error\r\n  function h% (x) as integer\r\n                  ^", None),
}

# A record in a record, copied whole: its field's offset was dropped, so o.i = x wrote o's start
# (" 7  0").
CASES |= {
    "qb_rec_nested_copy": (bas("TYPE Inner", "v AS INTEGER", "END TYPE", "TYPE Outer", "a AS INTEGER", "i AS Inner",
                               "END TYPE", "DIM o AS Outer, x AS Inner", "x.v = 7: o.a = 1", "o.i = x",
                               "PRINT o.a; o.i.v", "o.i.v = 5: x = o.i: PRINT x.v"),
                           " 1  7 \r\n 5 \r\n", "ERROR"),
}

# DIM a(), ours, was a Syntax error: a dynamic array with no elements until REDIM, as ERASE leaves
# one. A procedure's goes when it returns; a STATIC one's elements stay.
CASES |= {
    "dim_empty": (bas("dim a() as integer", "on error goto h", "print lbound(a)",
                      "redim a(1 to 3)", "a(3) = 7: print a(3); ubound(a)", "erase a", "redim a(2): print ubound(a)",
                      "dim a()", "s: s: f& = FRE(-1): t: t: print FRE(-1) = f&", "end", OV,
                      "sub s static", "dim b() as string", "redim b(2): b(2) = \"x\"", "end sub",
                      "sub t", "dim c() as long", "redim c(100000)", "end sub"),
                  "ov 9 \r\n 7  3 \r\n 2 \r\nov 10 \r\n-1 \r\n", "ERROR"),
}

# LBOUND and UBOUND of a record array read another array of its name, made 0 TO 10 (" 0  10").
CASES |= {
    "qb_rec_array_bounds": (bas("TYPE Vec", "x AS SINGLE", "END TYPE", "DIM r(2 TO 5) AS Vec", "r(3).x = 1",
                                "PRINT LBOUND(r); UBOUND(r)"), " 2  5 \r\n", "ERROR"),
}

# a(:) = b(:), ours, was a Syntax error: a gets b's bounds and its own copy of the elements, strings
# too; a DIM'd array keeps its bounds (Duplicate definition).
CASES |= {
    "array_copy": (bas("type Vec", " x as single", "end type", "declare sub show (a() as long)",
                       "dim a() as long, b() as long, s() as string, t() as string, r(2) as Vec, q() as Vec",
                       "dim f(3) as long", "redim a(2 to 4): a(2) = 1: a(3) = 2: a(4) = 3",
                       "b(:) = a(:): a(3) = 99: show b(): show a()",
                       'redim s(1 to 2): s(1) = "one": s(2) = "two"', 't(:) = s(:): s(1) = "uno": print t(1); t(2); s(1)',
                       "r(1).x = 5: q(:) = r(:): r(1).x = 6: print q(1).x; lbound(q); ubound(q)",
                       "b(:) = b(:): show b()", "dim e() as long: on error goto h: b(:) = e(:): print ubound(b)",
                       "let a(:) = f(:): show a()", "f(:) = a(:)", "end", OV,
                       "sub show (a() as long)", " for i = lbound(a) to ubound(a): print a(i);: next: print", "end sub"),
                   " 1  2  3 \r\n 1  99  3 \r\nonetwouno\r\n 5  0  2 \r\n 1  2  3 \r\nov 9 \r\n 0  0  0  0 \r\nov 10 ",
                   "ERROR"),
    "array_copy_type": (bas("dim a() as long, s() as string", "s(:) = a(:)"),
                        ".BAS:2: Type mismatch\r\n  s(:) = a(:)\r\n         ^", None),
}

# a() = b(), ours, copied: it moves b's array, no element copied, and leaves b empty, as ERASE does.
# One its call outlives (a frame's, an arena's) is copied to the heap first: g() got a dead frame's.
# A heap one moved into a local is freed at its return; SWAP of records takes a frame's off too.
CASES |= {
    "array_move": (bas("type Box", " a() as long", "end type", "declare sub show (a() as long)",
                       "declare sub fromframe ()", "declare sub fromarena ()", "declare sub intoparam (x() as long)",
                       "declare sub intolocal ()", "declare sub swapped ()", "declare sub scrub ()",
                       "dim shared g() as long, gb as Box", "dim a() as long, b() as long, hb as Box",
                       "redim a(2 to 4): a(2) = 1: a(3) = 2: a(4) = 3: p& = varptr(a(3))",
                       "b() = a(): print varptr(b(3)) = p&: show b()", "on error goto h: print a(3): on error goto 0",
                       "b() = b(): show b()",
                       "call fromframe: scrub: show g()", "call fromarena: scrub: show g()",
                       "call intoparam(a()): scrub: show a()", "call swapped: scrub: show gb.a()", "hb.a = gb.a: gb.a(1) = 6: print hb.a(1); gb.a(1)",
                       "redim g(1000): call intolocal: f& = fre(-1)",
                       "for i = 1 to 50: redim g(1000): intolocal: next: print f& - fre(-1)", "end", OV,
                       "sub show (a() as long)", " for i = lbound(a) to ubound(a): print a(i);: next: print", "end sub",
                       "sub fromframe", " dim l(1 to 3) as long: l(1) = 4: l(2) = 5: l(3) = 6: g() = l()", "end sub",
                       "sub fromarena", " dim l() as long: redim l(1 to 2): l(1) = 7: l(2) = 8: g() = l()", "end sub",
                       "sub intoparam (x() as long)", " dim l() as long: redim l(0 to 1): l(0) = 9: l(1) = 10: x() = l()",
                       "end sub", "sub swapped", " dim rs() as Box, l(1 to 2) as long: redim rs(1 to 1): l(1) = 11: l(2) = 12",
                       " rs(1).a() = l(): swap rs(1), gb", "end sub", "sub intolocal", " dim l() as long: l() = g()", "end sub",
                       "sub scrub", " dim z(1 to 64) as long, y() as long: redim y(1 to 64)",
                       " for i = 1 to 64: z(i) = -1: y(i) = -2: next", "end sub"),
                   "-1 \r\n 1  2  3 \r\nov 9 \r\n 1  2  3 \r\n 4  5  6 \r\n 7  8 \r\n 9  10 \r\n 11  12 \r\n"
                   " 11  6 \r\n 0 \r\n", "ERROR"),
    # a() = b(:) or a(:) = b(): which is meant is not known.
    "array_move_mixed": (bas("dim a() as long, b() as long", "a() = b(:)"), ".BAS:2: Syntax error", None),
    "array_copy_mixed": (bas("dim a() as long, b() as long", "a(:) = b()"), ".BAS:2: Syntax error", None),
}

# A DECLARE's line was not checked: a keyword for a parameter's name passed it, the procedure left
# undeclared, and the error came at its call. QB's forms of it, SEG, AS ANY, CDECL, ALIAS, still pass.
CASES |= {
    "declare_keyword_param": (bas("declare function F% (a as integer, point as integer)", "print F%(1, 2)",
                                  "function F% (a as integer, point as integer)", "F% = a + point", "end function"),
                              ".BAS:1: Syntax error\r\n  declare function F% (a as integer, point as integer)\r\n"
                              + " " * 37 + "^", None),
    "declare_qb_forms": (bas("declare sub s (x as any)", "declare sub c cdecl (byval x as integer)",
                             'declare sub d alias "dd" (seg x as integer)', "s 5", "sub s (x as integer)", " print x",
                             "end sub"), " 5 \r\n", "ERROR"),
}

# FUNCTION f (..) AS type(), ours: its result an array, the caller's hidden one passed, emptied as the
# FUNCTION starts; a() = f(..) moves it, f(..) passes it. In f, f(i) is its result's element.
SQUARES = ("function Squares (n as integer) as long()", " redim Squares(1 to n)",
           " for i = 1 to n: Squares(i) = i * i: next", " if n > 1 then Squares(1) = Squares(2) + ubound(Squares)",
           "end function")
CASES |= {
    "func_as_array": (bas("type Vec", " x as single", "end type", "declare sub show (a() as long)",
                          "declare sub t ()", "dim s() as long, w() as string, p() as Vec",
                          "s() = Squares(4): show s()", "show Squares(3)", "w() = Names(): print w(1); w(2); ubound(w)",
                          "p() = Pts(2): print p(1).x; p(2).x", "for k% = 1 to 3: s() = Squares(k%): next: show s()",
                          "for k = 1 to 50: show Squares(1): if k = 1 then f& = fre(-1)", "next: print f& - fre(-1)",
                          "t: f& = fre(-1): t: t: print f& - fre(-1)", "end",
                          "sub t", " dim u() as long", " u() = Squares(1000): show2 Squares(2000)", "end sub",
                          "sub show2 (a() as long): end sub",
                          "sub show (a() as long)", " for i = lbound(a) to ubound(a): print a(i);: next: print", "end sub",
                          *SQUARES, "function Names () as string()", " redim Names(1 to 2)",
                          ' Names(1) = "ab": Names(2) = "cd"', "end function", "function Pts (n%) as Vec()",
                          " redim Pts(1 to n%)", " Pts(1).x = 7: Pts(n%).x = 9", "end function"),
                      " 8  4  9  16 \r\n 7  4  9 \r\nabcd 2 \r\n 7  9 \r\n 7  4  9 \r\n" + " 1 \r\n" * 50
                      + " 0 \r\n 0 \r\n", "ERROR"),
    "func_as_array_value": (bas("print Squares(3)", *SQUARES), ".BAS:1: Type mismatch\r\n  print Squares(3)", None),
    "func_as_array_fixed": (bas("dim s(5) as long", "on error goto h", "s() = Squares(3)", "end", OV, *SQUARES),
                            "ov 10 ", "ERROR"),
    "func_as_array_record": (bas("type Vec", " x as single", "end type", "show Mk", "sub show (a() as long): end sub",
                                 "function Mk () as Vec", "end function"), ".BAS:4: Parameter type mismatch", None),
}

# A fixed array in a TYPE, ours, was a Syntax error: inline, its first subscript the fastest; its
# element a field, a record, an argument; out of its bounds Subscript out of range, a constant's
# when compiled. A subscript with an operator left the statement at its end: Syntax error.
POLY = ("type Vec", " x as single", " y as single", "end type", "type Poly", " n as integer",
        " pts(1 to 4) as Vec", " w(2, 1 to 3) as integer", " tag(3) as string * 2", "end type",
        "dim p as Poly")
CASES |= {
    "field_array": (bas(*POLY, "declare sub showv (v as Vec)", "declare sub inc (k as integer)",
                        "declare function mk () as Poly", "dim q as Poly, ps(3) as Poly", "on error goto h",
                        "p.pts(2).x = 5: p.w(1, 2) = 7: i = 2: p.pts(i).y = p.pts(2).x + 1",
                        'p.tag(3) = "ab": p.n = 1', "print p.pts(2).x; p.pts(i).y; p.w(1, 2); len(p); p.tag(3); p.n",
                        "q = p: p.pts(2).x = 0: print q.pts(2).x; q.w(1, 2)",
                        "ps(1).w(2, 3) = 9: print ps(1).w(2, 3); ps(2).w(2, 3)",
                        "showv q.pts(2): inc p.w(1, 2): print p.w(1, 2)", "q = mk: print q.pts(i + 1).x",
                        "i = 5: print p.pts(i).x", "j = 0: print p.w(1, j)", "end", OV,
                        "sub showv (v as Vec): print v.x: end sub", "sub inc (k as integer): k = k + 1: end sub",
                        "function mk () as Poly", " mk.pts(3).x = 4", "end function"),
                    " 5  6  7  80 ab 1 \r\n 5  7 \r\n 9  0 \r\n 5 \r\n 8 \r\n 4 \r\nov 9 \r\nov 9 \r\n", "ERROR"),
    "field_array_bound": (bas(*POLY, "p.pts(5).x = 1"), ".BAS:12: Subscript out of range", None),
    "field_array_dims": (bas(*POLY, "p.w(1) = 2"), ".BAS:12: Subscript out of range", None),
    "field_array_whole": (bas(*POLY, "p.pts.x = 2"), ".BAS:12: Syntax error", None),
}

# A TYPE's fixed array bound naming a CONST was a Syntax error. Each TYPE's LEN is its CONST's value,
# as the run computes the same expression.
CONST_BOUNDS = {"4": 4, "2 + 3 * 4": 14, "4 \\ 2 * 2": 1, "17 mod 5 * 2": 7, "-(2 - 9)": 7,
                "(1 + 2) * (3 + 4) \\ 2": 10, "&H10": 16, "k% * 2": 6, "20 - 3 - 4": 13, "7 \\ 2 \\ 2": 1}
CASES |= {
    "field_array_const": (bas("const k% = 3", *(line for i, e in enumerate(CONST_BOUNDS) for line in (
                              f"const c{i} = {e}", f"type t{i}", f" a(1 to c{i}) as byte", "end type",
                              f"dim x{i} as t{i}", f"print len(x{i}); {e}"))),
                          "".join(f" {v}  {v} \r\n" for v in CONST_BOUNDS.values()), "ERROR"),
    "field_array_const_dims": (bas("const n = 4, lo = -1", "const m = n * 2 + (n \\ 2) - 1 mod 3", "type U",
                                   " a(1 to n) as integer", " b(lo to m, 2) as byte", "end type", "dim x as U",
                                   "x.a(n) = 5: x.b(m, 2) = 7: print x.a(4); x.b(9, 2); len(x)"),
                               " 5  7  49 \r\n", "ERROR"),
}

# A STRING in a TYPE, ours, was 0 bytes: the field read "" and its writes were lost. Each copy of a
# record, an array of them or a nested one owns its strings; GET and PUT refuse such a TYPE.
ITEM = ("type Item", " nm as string", " tags(1 to 2) as string", " qty as long", "end type",
        "type Box", " it as Item", " its(1) as Item", "end type")
CASES |= {
    "field_string": (bas(*ITEM, "declare sub greet (s as string)", "declare sub t ()",
                         "dim a as Item, b as Item, bx as Box, cx as Box, xs(2) as Item, ys() as Item",
                         'a.nm = "apple": a.tags(1) = "red": a.tags(2) = "sweet": a.qty = 3',
                         'b = a: a.nm = "pear": a.tags(1) = "green"',
                         'print b.nm; " "; b.tags(1); " "; b.tags(2); b.qty', 'print a.nm; " "; a.tags(1); len(a.nm)',
                         'bx.it = a: bx.its(1).nm = "fig": cx = bx: bx.it.nm = "x": bx.its(1).nm = "y"',
                         'print cx.it.nm; " "; cx.its(1).nm',
                         'xs(1) = b: xs(1).tags(2) = "sour": print xs(1).nm; " "; xs(1).tags(2); " "; b.tags(2)',
                         'ys(:) = xs(:): xs(1).nm = "z": print ys(1).nm; " "; ys(1).tags(2)',
                         "redim preserve ys(5): print ys(1).nm", "greet a.nm: print a.nm",
                         'swap a.nm, b.nm: print a.nm; " "; b.nm',
                         'for i = 1 to 2000: b.nm = string$(100, "q"): next',
                         'print a.nm; " "; b.tags(1); " "; cx.its(1).nm; " "; ys(1).tags(2)',
                         "t: t: f& = fre(-1): for i = 1 to 100: t: next: print fre(-1) = f&", "end",
                         'sub greet (s as string): s = "hello " + s: end sub',
                         'sub t: dim r as Item: r.nm = string$(1000, "x"): r.tags(2) = r.nm: end sub'),
                     "apple red sweet 3 \r\npear green 4 \r\npear fig\r\napple sour sweet\r\napple sour\r\n"
                     "apple\r\nhello pear\r\napple hello pear\r\napple red fig sour\r\n-1 \r\n", "ERROR"),
    "field_string_put": (bas(*ITEM, "dim a as Item", 'open "x" for binary as 1: put 1, , a'),
                         ".BAS:11: Type mismatch", None),
}

# A dynamic array in a TYPE, ours, was a Syntax error: empty until REDIM, copied with its record,
# freed with it; its header names its cell, so the heap's compaction moves records that own arrays.
MESH = ("type Vec", " x as single", " y as single", "end type", "type Mesh", " title as string",
        " pts() as Vec", " tags() as string", " kids() as Mesh", "end type", "dim m as Mesh")
CASES |= {
    "field_dynamic": (bas(*MESH, "declare sub fill (a() as Vec, n as integer)",
                          "declare function mk (n as integer) as Mesh",
                          "dim c as Mesh, a() as Vec, rs() as Mesh", "on error goto h",
                          'm.title = "root": redim m.pts(1 to 3)',
                          "for i = 1 to 3: m.pts(i).x = i: m.pts(i).y = i * 10: next",
                          "print lbound(m.pts); ubound(m.pts()); m.pts(2).x; m.pts(3).y",
                          "redim preserve m.pts(1 to 5): m.pts(5).x = 55: print ubound(m.pts); m.pts(3).y; m.pts(5).x",
                          'redim m.tags(2): m.tags(0) = "a": m.tags(2) = "c" + "c": print m.tags(0); m.tags(2); len(m.tags(1))',
                          "a(:) = m.pts(:): a(1).x = 99: print ubound(a); a(5).x; m.pts(1).x",
                          'redim m.kids(1 to 2): m.kids(2).title = "kid": redim m.kids(2).pts(0 to 1): m.kids(2).pts(1).y = 7',
                          'c = m: c.kids(2).pts(1).y = 8: c.tags(0) = "z"',
                          "print m.kids(2).pts(1).y; c.kids(2).pts(1).y; m.tags(0); c.tags(0); ubound(c.kids(2).pts); c.title",
                          "erase m.pts: m.kids(1).pts(:) = a(:): print ubound(m.kids(1).pts); m.kids(1).pts(1).x",
                          "print ubound(m.pts)", "fill m.pts(), 4: print ubound(m.pts); m.pts(4).y",
                          "c = mk(3): print ubound(c.kids); c.kids(3).title; ubound(c.kids(3).pts)", "print ubound(c.pts)",
                          'redim rs(1 to 2): redim rs(2).tags(1): rs(2).tags(1) = "t": redim preserve rs(0 to 9): print rs(2).tags(1)',
                          "end", OV, "sub fill (a() as Vec, n as integer)", " redim a(1 to n): a(n).y = n * n", "end sub",
                          "function mk (n as integer) as Mesh", " redim mk.kids(1 to n)",
                          " for i = 1 to n: mk.kids(i).title = str$(i): redim mk.kids(i).pts(i): next", "end function"),
                      " 1  3  2  30 \r\n 5  30  55 \r\nacc 0 \r\n 5  55  1 \r\n 7  8 az 1 root\r\n 5  99 \r\nov 9 \r\n"
                      " 4  16 \r\n 3  3 3 \r\nov 9 \r\nt\r\n", "ERROR"),
    # Locals, results and copies freed: no bytes lost a round; the same keeping its arrays lost them.
    "field_dynamic_free": (bas(*MESH, "declare sub scratch (n as integer)", "declare function mk (n as integer) as Mesh",
                               "dim keep as Mesh",
                               "for k = 1 to 200", " scratch 5: dim q as Mesh: q = mk(5): q.kids(2) = q.kids(1): erase q.kids",
                               " if k = 2 then f0& = fre(-1)", "next", "f1& = fre(-1): redim keep.kids(1 to 500)",
                               "print f0& - f1&; f1& - fre(-1) > 1000", "end",
                               "sub scratch (n as integer)", " dim t as Mesh: redim t.kids(1 to n)",
                               ' for i = 1 to n: redim t.kids(i).kids(1 to n): t.kids(i).kids(n).title = string$(100, "z"): next',
                               "end sub", "function mk (n as integer) as Mesh", " redim mk.kids(1 to n)",
                               " for i = 1 to n: redim mk.kids(i).pts(1 to 3): mk.kids(i).title = str$(i): next",
                               "end function"),
                           " 0 -1 \r\n", "ERROR"),
    # A local record array kept in its frame: its records' arrays were never freed, 176616 bytes in 198 calls.
    "field_dynamic_frame_free": (bas(*MESH, "declare sub frame (n as integer)",
                                     "for k = 1 to 200", " frame 5", " if k = 2 then f0& = fre(-1)", "next",
                                     "print f0& - fre(-1)", "end", "sub frame (n as integer)", " dim rs(2) as Mesh",
                                     " redim rs(1).pts(1 to 100): rs(1).pts(n).x = n",
                                     ' redim rs(2).tags(n): rs(2).tags(n) = string$(50, "t")', "end sub"),
                                 " 0 \r\n", "ERROR"),
    # Its array fields were not empty: UBOUND made one of 10, as an undeclared array's.
    "field_dynamic_frame_empty": (bas(*MESH, "declare sub frame ()", "frame", "end", "sub frame",
                                      " dim q(2) as Mesh", " print ubound(q(0).pts)", "end sub"),
                                  "Subscript out of range in line", None),
    # A returned call's leaked array, its owner cell in the frame gone: compaction wrote it into the
    # next call's record there, which then held the dead call's 7 elements.
    "field_dynamic_frame_dead": (bas(*MESH, "declare sub t ()", "declare sub u ()", "dim shared early() as long",
                                     "redim early(1000)", "on error goto h", "t", "u", "end", OV, "sub t",
                                     " dim rs(2) as Mesh", " redim rs(0).pts(7): rs(0).pts(3).x = 33", "end sub",
                                     "sub u", " dim q(2) as Mesh", " erase early", " print ubound(q(0).pts)",
                                     "end sub"),
                                 "ov 9 \r\n", "ERROR"),
    # Records that own arrays and strings, moved by REDIMs around them 1000 times and more.
    "field_dynamic_moves": (bas("type Cell", " v as long", " xs() as long", " s as string", "end type",
                                "dim rs() as Cell, junk() as long, last(1 to 20) as long", "redim rs(1 to 20)",
                                "for k = 1 to 3000", " i = k mod 20 + 1", " redim preserve rs(i).xs(k mod 13)",
                                " rs(i).xs(k mod 13) = k: rs(i).s = str$(k): last(i) = k", " redim junk(k mod 97 * 10)",
                                " if k mod 50 = 0 then redim preserve rs(1 to 20 + k mod 3): "
                                "if ubound(rs) > 20 then redim rs(ubound(rs)).xs(500)",
                                "next", "for i = 1 to 20",
                                " if rs(i).xs(last(i) mod 13) = last(i) and val(rs(i).s) = last(i) then ok = ok + 1",
                                "next", "print ok", "redim preserve rs(1 to 22): f& = fre(-1)",
                                "for k = 1 to 100: redim rs(22).xs(500): redim preserve rs(1 to 21): redim preserve rs(1 to 22): next",
                                "print fre(-1) = f&"),
                            " 20 \r\n-1 \r\n", "ERROR"),
    # FE.ADDR's element from the header: n dimensions, lower bounds, its size by the field's type;
    # past a bound or with too few subscripts arry_ref's error.
    "field_dynamic_index": (bas("type R", " w as integer", " m() as integer", " s() as string", "end type",
                                "dim r as R, a(1 to 3, -2 to 2) as integer", "redim r.m(1 to 3, -2 to 2): redim r.s(4)",
                                "for i = 1 to 3: for j = -2 to 2: r.m(i, j) = i * 10 + j: a(i, j) = i * 10 + j: next: next",
                                "for i = 1 to 3: for j = -2 to 2: if r.m(i, j) = a(i, j) then ok = ok + 1", "next: next",
                                'r.s(4) = "four": r.m(3, 2) = r.m(3, 2) + 1',
                                "print ok; r.m(3, 2); r.s(4); varptr(r.m(2, -1)) - varptr(r.m(1, -2)); "
                                "varptr(r.m(2, 1)) - varptr(r.m(1, -2)); varptr(r.s(3)) - varptr(r.s(1))",
                                "on error goto h", "print r.m(4, 0)", "print r.m(1)", "end", OV),
                            " 15  33 four 16  40  12 \r\nov 9 \r\nov 9 \r\n", "ERROR"),
    "field_dynamic_empty": (bas(*MESH, "m.pts(1).x = 1"), "Subscript out of range in line", None),
    # A field in a REDIM's or UBOUND's subscripts was taken whole: Type mismatch at t.d(1).
    "field_whole_subscript": (bas("type R", " xs() as long", "end type", "type H", " d() as long", "end type",
                                  "dim rs(3) as R, t as H", "redim t.d(2): t.d(1) = 2",
                                  "redim rs(t.d(1)).xs(4): print ubound(rs(t.d(1)).xs); ubound(rs(2).xs)"),
                              " 4  4 \r\n", "ERROR"),
    # A fixed array field whole, ours, was refused: LBOUND, UBOUND and ERASE a Syntax error, GET into
    # it Illegal function call. Its bounds are its TYPE's; ERASE zeroes it, as a DIM'd array's.
    "field_fixed_whole": (bas("const N = 4", "type T", " at(N) as long", " g(N * 100 - 1) as long",
                              " m(1 to 3, -2 to 2) as integer", " s(2) as string", " d() as long", "end type",
                              "declare sub chk (r as T)", "dim t as T, rs(2) as T", "on error goto h",
                              "print lbound(t.at); ubound(t.at); ubound(t.g()); lbound(t.m, 2); ubound(t.m, 1)",
                              "k = 2: print lbound(t.m, k); ubound(rs(1).m, k): print ubound(t.m, 3)",
                              't.at(2) = 5: t.s(1) = "x": erase t.at, t.s: print t.at(2); len(t.s(1))',
                              "screen 13: pset (3, 3), 7: redim t.d(399)",
                              "get (0, 0)-(7, 7), t.g(20): get (0, 0)-(7, 7), t.d(20)",
                              "print t.g(20) = t.d(20); t.g(21) = t.d(21); t.g(25) = t.d(25)",
                              "cls: put (10, 10), t.g(20): print point(13, 13)", "get (0, 0)-(63, 63), t.at(0)",
                              "t.g(399) = 9: chk t: print t.g(399)", "end", OV,
                              "sub chk (r as T)", " print ubound(r.g); r.g(399): erase r.g", "end sub"),
                          " 0  4  399 -2  3 \r\n-2  2 \r\nov 9 \r\n 0  0 \r\n-1 -1 -1 \r\n 7 \r\nov 5 \r\n"
                          " 399  9 \r\n 0 \r\n", "ERROR"),
    # REDIM of a fixed array field said Subscript out of range: its size is its TYPE's.
    "field_fixed_redim": (bas("type T", " g(4) as long", "end type", "dim t as T", "redim t.g(5)"),
                          ".BAS:5: Duplicate definition", None),
    # A fixed array field passed whole to an array parameter was a Syntax error. The parameter is the
    # field: from a record, a record array's element, a local record, a record parameter, passed on.
    "field_fixed_arg": (bas("type T", " a as integer", " g(1 to 4) as long", " m(2, 1) as integer", "end type",
                            "declare sub show (x() as long)", "declare sub deep (x() as long)",
                            "declare sub mat (m() as integer)", "declare sub inner (r as T)",
                            "dim t as T, rs(2) as T", "t.g(3) = 30: show t.g(): print t.g(1)",
                            "rs(1).m(2, 1) = 5: mat rs(1).m()", "inner t", "end",
                            "sub show (x() as long)", " print lbound(x); ubound(x); x(3)", " x(1) = 11: deep x()",
                            "end sub", "sub deep (x() as long)", " print ubound(x); x(1)", "end sub",
                            "sub mat (m() as integer)", " print ubound(m, 1); ubound(m, 2); m(2, 1)", "end sub",
                            "sub inner (r as T)", " dim u as T", " u.g(3) = 3: show u.g(): show r.g()",
                            " print u.g(1); r.g(1)", "end sub"),
                        " 1  4  30 \r\n 4  11 \r\n 11 \r\n 2  1  5 \r\n 1  4  3 \r\n 4  11 \r\n"
                        " 1  4  30 \r\n 4  11 \r\n 11  11 \r\n", "ERROR"),
    # The field's record moved as the heap shrank under it during the call: the parameter follows.
    # REDIM of it is refused, ERASE zeroes it, a() = it copies it: its record keeps it. Its cell's room
    # comes back as the call ends.
    "field_fixed_arg_moves": (bas("type T", " g(4) as long", "end type",
                                  "declare sub grow (x() as long, b() as long)", "declare sub keep (x() as long)",
                                  "redim big(5000) as long", "dim t as T", "on error goto h",
                                  "grow t.g(), big(): print t.g(2); t.g(3)",
                                  "for i = 1 to 2000: keep t.g(): next", "end", OV,
                                  "sub grow (x() as long, b() as long)", " x(3) = 3: erase b: x(2) = 7: keep x()",
                                  "end sub", "sub keep (x() as long)", " if x(4) then exit sub",
                                  " dim y() as long", " y() = x()", " print ubound(y); y(2); x(2)", " redim x(9)",
                                  " erase x: print x(2): x(4) = 1", "end sub"),
                              " 4  7  7 \r\nov 10 \r\n 0 \r\n 0  0 \r\n", "ov 7"),
    "field_dynamic_dims": (bas(*MESH, "m.pts(" + ",".join("0" * 61) + ").x = 1"), ".BAS:12: Too many dimensions", None),
    "field_dynamic_type": (bas(*MESH, "dim s() as Mesh", "s(:) = m.pts(:)"), ".BAS:13: Type mismatch", None),
    "field_dynamic_value": (bas(*MESH, "print m.pts"), ".BAS:12: Type mismatch", None),
    "field_dynamic_put": (bas(*MESH, 'open "x" for binary as 1: put 1, , m'), ".BAS:12: Type mismatch", None),
    # PUT's refusal said a string was where a number is needed.
    "field_dynamic_put_words": (bas(*MESH, 'open "x" for binary as 1: put 1, , m'),
                                "^\r\n`m` holds a variable-length STRING or a dynamic array", None),
    # VARPTR of such a record was PUT's Type mismatch; BC's takes any variable.
    "field_dynamic_varptr": (bas(*MESH, "declare sub t ()", "dim rs(2) as Mesh",
                                 "print varptr(m) > 0; varptr(rs(2)) - varptr(rs(0)) = 2 * len(m)", "t", "end",
                                 "sub t", " dim q(1) as Mesh", " print varptr(q(1)) - varptr(q(0)) = len(q(0))",
                                 "end sub"),
                             "-1 -1 \r\n-1 \r\n", "ERROR"),
}

# SWAP of two records was a Type mismatch.
CASES |= {
    "swap_records": (bas("type P", " n as integer", " s as string * 3", "end type", "dim a as P, b as P, r(1 to 2) as P",
                         'a.n = 1: a.s = "abc": b.n = 2: b.s = "xyz"', "swap a, b: print a.n; a.s; b.n; b.s",
                         "r(1) = a: swap r(1), b: print r(1).n; r(1).s; b.n; b.s"),
                     " 2 xyz 1 abc\r\n 1 abc 2 xyz\r\n", "ERROR"),
    "swap_records_type": (bas("type P", " n as integer", "end type", "type Q", " n as integer", "end type",
                              "dim a as P, c as Q", "swap a, c"), ".BAS:8: Type mismatch", None),
    # Their STRINGs' and arrays' owners follow, else the heap's compaction writes each into the
    # other's cell; a local's arena array copied to the heap, else it dies with its call.
    "swap_records_dynamic": (bas(*MESH, "declare sub swapin (x as Mesh, n as integer)", "declare sub scrub ()",
                                 "declare sub inarr (x as Mesh, n as integer)",
                                 "dim c as Mesh, early() as long, rs() as Mesh", "redim early(1000)",
                                 'g$ = string$(100, "g"): m.title = string$(2, "m"): redim m.pts(1 to 2): m.pts(2).x = 2',
                                 'redim m.tags(1): m.tags(1) = string$(2, "t")',
                                 'c.title = string$(2, "c"): redim c.pts(1 to 3): c.pts(3).x = 3',
                                 'redim c.kids(1 to 1): c.kids(1).title = "ck"',
                                 "redim rs(1 to 2): redim rs(1).pts(5): rs(1).pts(5).x = 15: redim rs(2).pts(7): rs(2).pts(7).x = 27",
                                 "swap m, c: swap rs(1), rs(2)",
                                 'g$ = "": erase early: x& = fre(-1): h$ = string$(300, "h")',
                                 "print m.title; ubound(m.pts); m.pts(3).x; m.kids(1).title; c.title; ubound(c.pts); c.pts(2).x; c.tags(1)",
                                 "print ubound(rs(1).pts); rs(1).pts(7).x; ubound(rs(2).pts); rs(2).pts(5).x",
                                 "swapin m, 1: f& = fre(-1)", "for k% = 2 to 50: swapin m, k%: next", "call scrub: x& = fre(-1)",
                                 "print m.title; ubound(m.pts); m.pts(4).x; m.tags(2); fre(-1) - f&",
                                 "inarr m, 1: f& = fre(-1)", "for k% = 2 to 50: inarr m, k%: next", "call scrub: x& = fre(-1)",
                                 "print m.title; ubound(m.pts); m.pts(5).x; fre(-1) - f&", "end",
                                 "sub swapin (x as Mesh, n as integer)", " dim t as Mesh",
                                 ' t.title = "t" + chr$(64 + n): redim t.pts(1 to 4): t.pts(4).x = n',
                                 ' redim t.tags(2): t.tags(2) = string$(3, "q")', " swap t, x", "end sub",
                                 "sub scrub", " dim z(2000) as long", " for i = 0 to 2000: z(i) = -1: next", "end sub",
                                 "sub inarr (x as Mesh, n as integer)", " redim q(1 to 2) as Mesh",
                                 ' q(2).title = "q" + chr$(64 + n): redim q(2).pts(1 to 5): q(2).pts(5).x = n',
                                 " swap q(2), x", "end sub"),
                             "cc 3  3 ckmm 2  2 tt\r\n 7  27  5  15 \r\ntr 4  50 qqq 0 \r\nqr 5  50  0 \r\n", "ERROR"),
    # A heap array swapped into a local array's record was never freed: 12 bytes a call.
    "swap_records_arena_free": (bas("type T", " pts() as single", "end type", "declare sub give (x as T, n as integer)",
                                    "dim m as T", "give m, 1: f& = fre(-1)", "for k% = 2 to 50: give m, k%: next",
                                    "print fre(-1) - f&; ubound(m.pts); m.pts(3)", "end",
                                    "sub give (x as T, n as integer)", " redim q(1 to 2) as T", " swap q(2), x",
                                    " redim x.pts(1 to 3): x.pts(3) = n", "end sub"),
                                " 0  3  50 \r\n", "ERROR"),
}

# Graphics GET and PUT of a record's array field took it for an array named s.img: Illegal function
# call. They take the array's cell, a variable's, a parameter's or a field's.
CASES |= {
    "gfx_get_put_field": (bas("type Sprite", " w as integer", " img() as integer", "end type",
                              "declare sub show (b() as integer)", "declare sub own ()",
                              "dim s as Sprite, rs(1 to 2) as Sprite", "redim s.img(100): redim rs(2).img(200)",
                              "screen 13", "pset (1, 1), 9: pset (2, 3), 12",
                              "get (0, 0)-(3, 3), s.img", "put (20, 20), s.img",
                              "get (0, 0)-(3, 3), rs(2).img(50)", "put (40, 40), rs(2).img(50), pset",
                              "p = point(21, 21): q = point(22, 23): r = point(41, 41): t = point(42, 43)",
                              "screen 0", "print p; q; r; t; s.img(0) = rs(2).img(50)", "show s.img(): own", "end",
                              "sub show (b() as integer)",
                              " screen 13: pset (1, 1), 9: get (0, 0)-(3, 3), b: put (60, 60), b",
                              " p = point(61, 61): screen 0: print p; b(0) = 262176", "end sub",
                              "sub own", " dim l(50) as integer",
                              " screen 13: pset (2, 3), 12: get (0, 0)-(3, 3), l: put (60, 60), l",
                              " p = point(62, 63): screen 0: print p; l(0) = 262176", "end sub"),
                          " 9  12  9  12 -1 \r\n 9 -1 \r\n 12 -1 \r\n", "ERROR"),
}

# BC's 60 dimensions at most. Past 63 a DIM's count ran into its flags (REDIM b(64 bounds) made an
# array of none); past 64 an element's subscripts were written beyond their buffer.
DIMS = "(" + ",".join("0" * 60) + ")"
CASES |= {
    "dims_60": (bas(f"redim b{DIMS}", f"b{DIMS} = 7: print b{DIMS}; ubound(b, 60)"), " 7  0 \r\n", "ERROR"),
    "dims_61": (bas("redim b(" + ",".join("1" * 64) + ")", "print ubound(b, 1)"), ".BAS:1: Too many dimensions", None),
    "dims_61_element": (bas("dim a(3)", "a(" + ",".join("0" * 61) + ") = 1"), ".BAS:2: Too many dimensions", None),
    # A fixed array in a TYPE: past 16 dimensions a Syntax error.
    "dims_60_field": (bas("type T", f" w{DIMS} as byte", "end type", "dim p as T", f"p.w{DIMS} = 5: print p.w{DIMS}; len(p)"),
                      " 5  1 \r\n", "ERROR"),
    "dims_61_field": (bas("type T", " w(" + ",".join("0" * 61) + ") as byte", "end type"),
                      ".BAS:2: Too many dimensions", None),
}

# A TYPE over 65535 bytes was BC's TYPE more than 65535 bytes. Ours holds 16 MB: a field past 64 KB
# read, set, passed and copied as any other; past 16 MB, Out of memory (GET's and PUT's sizes).
CASES |= {
    "type_65535": (bas("type T", " a as string * 32767", " b as string * 32767", " c as byte", "end type",
                       "dim r as T", "print len(r)"), " 65535 \r\n", "ERROR"),
    "type_big": (bas("type Plain", " m(699, 23) as integer", " n as long", "end type", "type Big",
                     " m(699, 23) as integer", " s as string", " f as string * 5", " n as long", " d() as long",
                     "end type", "declare sub inc (x as long)", "dim b as Big, p as Plain, q as Plain, bs(1) as Big",
                     'b.s = "hi": b.f = "abc": b.n = 7: redim b.d(3): b.d(2) = 9: b.m(699, 23) = 5',
                     "i = 699: j = 23: inc b.n",
                     "print b.s; b.f; b.n; b.d(2); b.m(i, j); b.m(699, 23); len(p); len(b.f)",
                     "p.n = 4: p.m(1, 1) = 6: q = p: p.n = 0: print q.n; q.m(1, 1)", "swap p, q: print p.n; q.n",
                     'bs(1).n = 3: bs(1).s = "x": print bs(1).n; bs(1).s; bs(0).n',
                     'open "big.dat" for binary as 1: put 1, , p: q.n = 0: get 1, 1, q: close 1: print q.n',
                     "sub inc (x as long): x = x + 1: end sub"),
                 "hiabc   8  9  5  5  67204  5 \r\n 4  6 \r\n 4  0 \r\n 3 x 0 \r\n 4 \r\n", "ERROR"),
    "type_too_big": (bas("type T", " w(1 to 17000000) as byte", "end type"), ".BAS:2: Out of memory", None),
    # An error a declaration met was marked at the line's start, with no words of what is wrong; then
    # UBOUND's at UBOUND, its words naming it, not the array.
    "decl_error_name": (bas("print ubound(q)"), "`q` is used as an array here", None),
    "decl_error_words": (bas("type T", " a as byte", " w(" + ",".join("0" * 61) + ") as byte", "end type"),
                         ") as byte\r\n  ^\r\nAn array has at most 60 dimensions", None),
}

# IMPORT and FROM, ours: keywords only where they start such a statement, so a variable or SUB of
# either name still works; the statement is its line's first, at module level. PRIVATE only before SUB, FUNCTION,
# CONST or TYPE, PUBLIC only after DIM and before a name.
CASES |= {
    "import_words": (bas("private = 1: public = 2", "dim public(3), x as public", "print private; public",
                         "type public", " a as integer", "end type", "private const k = 4: print k",
                         "sub s", " private const j = 1", "end sub"),
                     "IMPORT_W.BAS:9: Syntax error", None),
    "import_names": (bas("import = 5: from = 2: print import; from", "import(1) = 3: print import(1)"),
                     " 5  2 \r\n 3 \r\n", "ERROR"),
    "import_sub": (bas("declare sub import (x)", "call import(3)", "end", "sub import (x)", " print x * 2", "end sub"),
                   " 6 \r\n", "ERROR"),
    "import_first": (bas("print 1: import gfx"), "IMPORT must be the first statement on its line", None),
    "import_in_sub": (bas("sub s", " from gfx import a", "end sub"), "`FROM` cannot be used inside a SUB or FUNCTION",
                      None),
    "import_form": (bas("import gfx."), "`gfx.` does not fit the IMPORT statement here", None),
    "import_from_star": (bas("from gfx import *, a"), "`,` does not fit the FROM statement here", None),
}


def beside(prog: str, files: dict[str, str]) -> tuple[str, str, str, dict[str, bytes]]:
    """A case run with files in its own folder, its modules found there."""
    return prog, "", "", {n: text.encode("latin-1") for n, text in files.items()}


SPR = {"lib/pkg/sprite.bas": bas("sub spr (x)", ' print "spr"; x', "end sub"),
       "lib/pkg/util.bas": bas("sub utl (x)", ' print "utl"; x', "end sub")}
# The modules an IMPORT or FROM names load once each, after the program's lines: the main program's
# folder, a package's, FROM's dots from the importing file's. IMPORT and FROM loaded nothing:
# each module's SUBs were Subprogram not defined.
CASES |= {
    "import_star": (beside(bas("from gfx import *", "plot 3", "print area(2)"),
                           {"gfx.bas": bas("sub plot (x)", ' print "plot"; x', "end sub",
                                           "function area (r)", " area = r * r", "end function")}),
                    "plot 3 \r\n 4 \r\n", "ERROR"),
    "import_package": (beside(bas("import lib.pkg.sprite", "from lib.pkg import util",
                                  "lib.pkg.sprite.spr 1: util.utl 2"), SPR),
                       "spr 1 \r\nutl 2 \r\n", "ERROR"),
    "import_relative": (beside(bas("import lib.pkg.rel", "lib.pkg.rel.go"),
                               {"lib/pkg/rel.bas": bas("from . import near", "from ..top import tp", "sub go",
                                                       " near.nr 1: tp 2", "end sub"),
                                "lib/pkg/near.bas": bas("sub nr (x)", ' print "nr"; x', "end sub"),
                                "lib/top.bas": bas("sub tp (x)", ' print "tp"; x', "end sub")}),
                        "nr 1 \r\ntp 2 \r\n", "ERROR"),
    # Imported twice, and in a cycle: loaded twice, its SUB was a Duplicate definition.
    "import_twice": (beside(bas("import ca", "from ca import *", "a 1: ca.a 3"),
                            {"ca.bas": bas("import cb", "sub a (x)", ' print "a"; x', " cb.b x + 1", "end sub"),
                             "cb.bas": bas("import ca", "sub b (x)", ' print "b"; x', "end sub")}),
                     "a 1 \r\nb 2 \r\na 3 \r\nb 4 \r\n", "ERROR"),
    # The main program's end, a false one-line IF's too, ran on into a module's top-level code.
    "import_main_ends": (beside(bas("import noisy", 'print "main"', 'if 0 then print "x"'),
                                {"noisy.bas": bas('print "noisy"', "sub nz", "end sub")}),
                         "noisy\r\nmain\r\n", "main\r\nnoisy"),
    "import_missing": (bas("print 1", "import nothere"), "IMPORT_M.BAS:2: File not found\r\n  import nothere", " 1"),
    "import_missing_name": (beside(bas("from lib.pkg import util, nothere"), SPR),
                            "IMPORT_M.BAS:1: File not found", None),
    # An error in a module: at its own file's line.
    "import_module_error": (beside(bas("import bad"), {"bad.bas": bas("sub b", "end sub", "x =")}),
                            "bad.bas:3: Syntax error\r\n  x =", None),
}

# Each file its own module: its variables, CONSTs, labels, line numbers, DEFtypes and OPTIONs. One
# file's leaked into the next: a variable was every file's, a module's CONST the main's too, a label
# in two Duplicate, the main's DEFINT, OPTION EXPLICIT and OPTION BASE the module's too.
CASES |= {
    "import_own_vars": (beside(bas("from cnt import *", "n = 10", "bump", "bump", "print n"),
                               {"cnt.bas": bas("dim shared n", "sub bump", " n = n + 1: print n", "end sub")}),
                        " 1 \r\n 2 \r\n 10 \r\n", "ERROR"),
    "import_own_const": (beside(bas("import kc", "k = 5: print k: kc.showk"),
                                {"kc.bas": bas("const k = 2", "sub showk", " print k", "end sub")}),
                         " 5 \r\n 2 \r\n", "ERROR"),
    "import_own_labels": (beside(bas("import lb", "gosub h", "10 lb.lbsub", "end", "h: print \"main h\"", "return"),
                                 {"lb.bas": bas("10 rem", "h: rem", "sub lbsub", ' print "lb"', "end sub")}),
                          "main h\r\nlb\r\n", "ERROR"),
    "import_other_line": (beside(bas("import ln", "goto 20"), {"ln.bas": bas("20 rem")}),
                          "IMPORT_O.BAS:2: Label not defined", None),
    "import_own_deftype": (beside(bas("defint a-z", "from fl import *", "x = 2.5: print x", "half 5"),
                                  {"fl.bas": bas("sub half (v)", " r = v / 2: print r", "end sub")}),
                           " 2 \r\n 2.5 \r\n", "ERROR"),
    "import_own_explicit": (beside(bas("option explicit", "from ex import *", "dim a: a = 1", "ex1"),
                                   {"ex.bas": bas("sub ex1", " y = 3: print y", "end sub")}),
                            " 3 \r\n", "ERROR"),
    # A module's declarations go before its importer's: main's DIM AS a module's TYPE was a Syntax
    # error, the module declared after the main.
    "import_type_first": (beside(bas("from tp import *", "dim p as pt", "p.x = 3: show p"),
                                 {"tp.bas": bas("type pt", " x as integer", "end type", "sub show (q as pt)",
                                                " print q.x", "end sub")}),
                          " 3 \r\n", "ERROR"),
    "import_own_base": (beside(bas("option base 1", "from bs import *", "dim m(2): print lbound(m)", "lb"),
                               {"bs.bas": bas("sub lb", " dim a(3): print lbound(a)", " z(2) = 1: print lbound(z)",
                                              "end sub")}),
                        " 1 \r\n 0 \r\n 0 \r\n", "ERROR"),
}

GFX = {"gfx.bas": bas("const maxw = 320", "type pt", " x as integer", " y as integer", "end type",
                      "sub plot (p as pt)", ' print "plot"; p.x; p.y', "end sub",
                      "function area (r)", " area = r * r", "end function")}
VM = {"vm.bas": bas("defint a-z", "dim public total", "dim public grid(5) as long", "dim public big as long",
                    "dim shared pages", "private const hid = 7",
                    "sub bump", " total = total + 1: grid(2) = grid(2) + 10", ' print "bump"; total; grid(2)',
                    "end sub", "private sub clip", "end sub")}
RC = {"rc.bas": bas("type pt", " x as integer", " y as integer", "end type", "dim public origin as pt",
                    "sub show", ' print "show"; origin.x; origin.y', "end sub")}
# What an import binds. Names were the whole program's: a module's SUB, FUNCTION and TYPE plain everywhere, its
# CONSTs and variables nowhere, a name two modules defined Duplicate; AS, mod.name, PRIVATE and DIM PUBLIC were
# Syntax errors or another name.
CASES |= {
    "import_from_names": (beside(bas("from gfx import plot as show, pt, maxw", "dim q as pt: q.x = maxw: show q"), GFX),
                          "plot 320  0 \r\n", "ERROR"),
    # A module's CONST, used before its CONST line compiled, read its variable, which its code never set: 0.
    "import_module_names": (beside(bas("import gfx", "dim p as gfx.pt", "p.x = 3", "gfx.plot p",
                                       "print gfx.area(3); gfx.maxw"), GFX),
                            "plot 3  0 \r\n 9  320 \r\n", "ERROR"),
    "import_star_own": (beside(bas("from gfx import *", "plot 1", "sub plot (x)", ' print "own"; x', "end sub"), GFX),
                        "own 1 \r\n", "ERROR"),
    # QB's multi-file programs DECLARE the SUBs another file has.
    "import_star_declared": (beside(bas("declare sub plot (p as any)", "from gfx import *", "dim q as pt", "plot q"),
                                    GFX),
                             "plot 0  0 \r\n", "ERROR"),
    "import_star_two": (beside(bas("from k1 import *", "from k2 import *", "print k"),
                               {"k1.bas": bas("const k = 1"), "k2.bas": bas("const k = 2")}),
                        "IMPORT_S.BAS:3: Duplicate definition", None),
    "import_listed_clash": (beside(bas("from gfx import maxw", "const maxw = 1"), GFX),
                            "IMPORT_L.BAS:2: Duplicate definition", None),
    "import_not_exported": (beside(bas("from vm import pages"), VM), "IMPORT_N.BAS:1: Name not exported", None),
    "import_private": (beside(bas("from vm import clip"), VM), "IMPORT_P.BAS:1: Name not exported", None),
    # A PRIVATE SUB's body started at the SUB, past the statement's start: a call to it was a Syntax error.
    "import_private_call": (beside(bas("from pv import go", "go"),
                                   {"pv.bas": bas("sub go", " hid 3", "end sub", "private sub hid (x)",
                                                  ' print "hid"; x', "end sub", "hid 4")}),
                            "hid 3 \r\n", "ERROR"),
    "import_no_reexport": (beside(bas("from ca import b"), {"ca.bas": bas("from cb import b"),
                                                             "cb.bas": bas("sub b", "end sub")}),
                           "IMPORT_N.BAS:1: Name not exported", None),
    "import_public": (beside(bas("from vm import total, grid(), bump", "total = 5", "bump", "print total; grid(2)"),
                             VM),
                      "bump 6  10 \r\n 6  10 \r\n", "ERROR"),
    # Its type the exporter's: big a LONG, total an INTEGER, whatever the importer's DEFtype.
    "import_public_type": (beside(bas("defsng a-z", "from vm import big, total", "big& = 70000: total% = 2",
                                      "print big; total"), VM),
                           " 70000  2 \r\n", "ERROR"),
    # FROM's name with its suffix: the exporter's of that type; another, as where used. The suffix was
    # the name's: Name not exported.
    "import_from_suffix": (beside(bas("from vm import total%, grid&(), big&, bump", "bump",
                                      "print total%; grid&(2); big&"), VM),
                           "bump 1  10 \r\n 1  10  0 \r\n", "ERROR"),
    "import_from_suffix_type": (beside(bas("from vm import total!"), VM), "IMPORT_F.BAS:1: Duplicate definition", None),
    "import_from_suffix_module": (beside(bas("from lib.pkg import util$"), SPR),
                                  "IMPORT_F.BAS:1: File not found", None),
    "import_public_suffix": (beside(bas("from vm import total", "total! = 1"), VM),
                             "IMPORT_P.BAS:2: Duplicate definition", None),
    # Its STRING * n looked up in the importer's scope, which has none: the string took any length.
    "import_public_fixed": (beside(bas("from fx import s, t", 's = "abcdef": t(2) = "abcdef"', 'print s; "|"; t(2)'),
                                   {"fx.bas": bas("dim public s as string * 4", "dim public t(3) as string * 3")}),
                            "abcd|abc\r\n", "ERROR"),
    "import_record_sub": (beside(bas("from rc import origin, show", "origin.x = 7", "s", "show", "sub s",
                                     " origin.y = 9", "end sub"), RC),
                          "show 7  9 \r\n", "ERROR"),
    "import_module_record": (beside(bas("import rc", "rc.origin.x = 5", "rc.show", "print rc.origin.x"), RC),
                             "show 5  0 \r\n 5 \r\n", "ERROR"),
    "import_module_taken": (beside(bas("import gfx", "gfx = 1"), GFX), "IMPORT_M.BAS:2: Duplicate definition", None),
    "import_module_missing": (beside(bas("import gfx", "print gfx.nothere"), GFX),
                              "IMPORT_M.BAS:2: Name not exported", None),
}

DA = {"da.bas": bas("data 1, 2", "sub rd", ' read x: print "a"; x', "end sub", "sub rs", " restore", "end sub"),
      "db.bas": bas("data 99")}
# A module's code runs at its first import, its ON ERROR and its DATA its own. Its code never ran: its variables
# stayed 0; ON ERROR was the program's, the last one run; READ went on through every file's DATA in turn.
CASES |= {
    "import_runs_code": (beside(bas("import a", "import a", "from b import total, hi", 'print "main"; total', "hi"),
                                {"a.bas": bas('print "a runs"', "import b", 'print "a again"'),
                                 "b.bas": bas('print "b runs"', "import a", "dim public total", "total = 5",
                                              "sub hi", ' print "hi"', "end sub", 'print "b end"')}),
                         "a runs\r\nb runs\r\nb end\r\na again\r\nmain 5 \r\nhi\r\n", "ERROR"),
    "import_module_end": (beside(bas("import e", 'print "no"'), {"e.bas": bas('print "e"', "end")}), "e\r\n", "no"),
    "import_on_error": (beside(bas("on error goto h", "from ea import boom", "from eb import bad", "boom", "bad",
                                   'print "after"', "end", 'h: print "main trapped"; err', "resume next"),
                               {"ea.bas": bas("on error goto h", "goto done", 'h: print "a trapped"; err',
                                              "resume next", "done:", "sub boom", " error 5", ' print "boom on"',
                                              "end sub"),
                                "eb.bas": bas("sub bad", " error 7", ' print "rest of bad"', "end sub")}),
                        "a trapped 5 \r\nboom on\r\nmain trapped 7 \r\nafter\r\n", "rest of bad"),
    "import_error_at_import": (beside(bas("on error goto h", "import zc", 'print "after"', "end",
                                          'h: print "trapped"; err', "resume next"),
                                      {"zc.bas": bas("error 9", 'print "zc rest"')}),
                               "trapped 9 \r\nafter\r\n", "zc rest"),
    "import_error_place": (beside(bas("import zb"), {"zb.bas": bas("x = 1", "y = x / 0")}),
                           "Division by zero in line No line number in module ZB ", None),
    "import_data": (beside(bas("from da import rd, rs", "import db", "data 10, 20", "read y: print y", "rd",
                               "read y: print y", "rd", "rs", "rd", "rd", "rd"), DA),
                    " 10 \r\na 1 \r\n 20 \r\na 2 \r\na 1 \r\na 2 \r\nOut of DATA", "a 99"),
}

CM = {"cm.bas": bas("type pt", " x as integer", "end type", "common shared x, y$, w(), o as pt",
                    "common shared /blk/ r", "sub show", " print x; y$; w(2); o.x; r", "end sub")}
# COMMON joins a program's files' variables by position, a named block's by its name. Each file's were its own:
# a module saw none of the main's values, and a CHAIN passed each file's in turn.
CASES |= {
    "import_common": (beside(bas("from cm import show", "type pt", " x as integer", "end type",
                                 "common shared /blk/ p", "common shared a, b$, v(), q as pt", "dim v(3)",
                                 'a = 1: b$ = "b": v(2) = 3: q.x = 4: p = 5', "show"), CM),
                      " 1 b 3  4  5 \r\n", "ERROR"),
    "import_common_type": (beside(bas("from cm import show", "common shared a$"), CM),
                           "IMPORT_C.BAS:2: Type mismatch", None),
    "import_common_array": (beside(bas("from cm import show", "common shared a, b$, v"), CM),
                            "IMPORT_C.BAS:2: Type mismatch", None),
    "import_common_record": (beside(bas("from cm import show", "type pt", " y as integer", "end type",
                                        "common shared a, b$, v(), q as pt"), CM),
                             "IMPORT_C.BAS:5: Type mismatch", None),
    # A variable two files name passes once, each way.
    "import_common_chain": (beside(bas("import ck", "common shared a, b$, c", 'a = 1: b$ = "b": c = 9',
                                       'chain "QB_CN1"'), {"ck.bas": bas("common shared x, y$")}),
                            " 1 b 9 \r\n", "ERROR"),
    "import_common_chained": (bas("common a, b$, c", 'a = 1: b$ = "b": c = 9', 'chain "QB_CN2"'),
                              " 1 b\r\n 9 \r\n", "ERROR"),
}


def recipes(page: Path) -> dict[str, tuple[tuple, str]]:
    """A help page's recipes: each main.bas, beside the files the page gave before it, and the lines
    after its `' prints:`, which must be all it prints."""
    files: dict[str, str] = {}
    out: dict[str, tuple[tuple, str]] = {}
    blocks = re.findall(r"^    ' (\S+\.bas)\n((?:(?!    ' \S+\.bas\n)    .*\n|\n)*)", page.read_text(), re.M)
    for name, body in blocks:
        lines = [line[4:] for line in body.rstrip("\n").split("\n")]
        if name != "main.bas":
            files[name] = bas(*lines)
            continue
        said = lines[lines.index("' prints:") + 1:]
        out[f"import_example_{len(out) + 1}"] = (beside(bas(*lines), dict(files)), bas(*(s[2:] for s in said)))
    return out


# The IMPORT examples page's: a recipe no longer printing what the page says.
EXAMPLES = recipes(ROOT / "help/ext/import-examples.md")


# Typed at the prompt, not run as a file.
REPL = {
    # A direct line ran from DCODE, its statement map's place: nothing printed.
    "repl_direct": ("PRINT 5\r\n", " 5 \r\n", "ERROR"),
    # A numbered line stepped 3 bytes past its M_INT, which is 5: the line lost its first bytes.
    "repl_numbered": ("10 PRINT 5\r\n20 PRINT 6\r\nRUN\r\n", " 5 \r\n 6 \r\n", "ERROR"),
}


# Companions the cases read, beside them.
FILES = {
    "WHERE.BI": b"' included\r\n\r\nY = 1 +\r\n",
    "SUB/A.BI": b"PRINT 1\r\nREM $INCLUDE: 'B.BI'\r\n",
    "SUB/B.BI": b"PRINT 2\r\n",
    "QB_CH3.BAS": b'COMMON q$, y()\r\nPRINT LEN(q$); RIGHT$(q$, 1); y(500)\r\n',
    "QB_CH4.BAS": b"COMMON m AS BYTE, y() AS BYTE\r\nPRINT m; LEN(m); y(2)\r\nm = m + 6\r\n",
    "QB_CH5.BAS": f"COMMON {', '.join(f'd{i}' for i in range(600))}, y() AS LONG\r\nPRINT d0; d599; y(30000)\r\n".encode(),
    "QB_CN1.BAS": bas("common d, e$, h", "print d; e$; h").encode(),
    "QB_CN2.BAS": bas("from qbcnm import pr", "common d, e$, h", "pr", "print h").encode(),
    "qbcnm.bas": bas("common shared k, l$", "sub pr", " print k; l$", "end sub").encode(),
    "QB_CH2.BAS": b'COMMON b%, q$, e#, y(), u AS SHORT\r\nPRINT b%; q$; e#; y(3); LBOUND(y); u; t\r\n'
                  b'PRINT #1, "kept": CLOSE #1\r\nON ERROR GOTO h\r\nCHAIN "NONE"\r\nEND\r\nh: PRINT "e"; ERR: RESUME NEXT\r\n',
}

# A forbidden "ERROR": any error's message at a line's start.
MSGS = dosrun.messages()


def forbidden(forbid: str, out: str) -> bool:
    lines = "\n" + out.replace("\r", "")
    if forbid == "ERROR":
        return any("\n" + m in lines for m in MSGS)
    if re.fullmatch(r" -?\d+", forbid):  # a number PRINTed: at a line's start, its space after
        return "\n" + forbid + " " in lines
    return forbid in out


def check_forbidden() -> None:
    """A forbidden " 1" matched the error's address ("at address 1732BAA0"): qb_blk_loop failed
    when the code moved, though PRINT 1 never ran."""
    assert not forbidden(" 1", "LOOP without DO in line No line number in module X at address 1732BAA0\r\n")
    assert forbidden(" 1", " 1 \r\nLOOP without DO in line")


def check_host_errors(tmp: Path) -> None:
    """A file named by a Unix path. Its compile error led with BC's runtime words ("Syntax error in
    line No line number in module /TMP at address 0000:075B", the module cut at the first '/'),
    and exited 0. Its runtime error named no line of the file, ended without a newline, exited 0,
    and its address was the bytecode offset split as a 16-bit seg:off. The runtime error's place
    goes to standard error: standard output stays BC's."""
    (tmp / "sub").mkdir()
    for name, prog, out, err, rc in (
        ("bad.bas", "PRINT 1\n\nX =\n", r"^sub/bad\.bas:3: Syntax error\r?\n  X =\r?\n", r"^$", 1),
        ("rt.bas", "PRINT 1\n\nPRINT 2: X = 1 / 0\n",
         r"\nDivision by zero in line No line number in module RT {7}at address [0-9A-F]{8}\r?\n\Z",
         r"^sub/rt\.bas:3\r?\n  print 2: X = 1 / 0\r?\n {11}\^\r?\n\Z", 1),
    ):
        (tmp / "sub" / name).write_text(prog)
        p = subprocess.run([str(dosrun.COM.resolve()), f"sub/{name}"], cwd=tmp, capture_output=True, text=True, timeout=10)
        got = (name, p.returncode, p.stdout, p.stderr)
        assert re.search(out, p.stdout) and re.search(err, p.stderr) and p.returncode == rc, got


# The compiler's fixed tables, each Out of memory or a Syntax error one past its size; a program
# grows until memory runs out.
BIG |= {
    # 2,048 block entries: 1,100 block IFs were Out of memory, with no line.
    "limit_blocks": (bas("n = 0", *["if n = 0 then", " n = 0", "end if"] * 1100, "print n"), " 0 \r\n", "ERROR"),
    # 64 blocks open: the 65th IF in was a Syntax error.
    "limit_nesting": (bas("n = 0", *["if n = 0 then"] * 100, "n = 7", *["end if"] * 100, "print n"), " 7 \r\n", "ERROR"),
    # 255 procedures, numbered by a byte: the 256th was Out of memory, with no line.
    "limit_procs": (bas(*[f"declare function f{i}% (x%)" for i in range(600)], "print f599%(1); f0%(2)",
                        *[x for i in range(600) for x in (f"function f{i}% (x%)", " dim a(3) as integer: a(1) = x%",
                                                          f" f{i}% = a(1) + {i}", "end function")]),
                    " 600  2 \r\n", "ERROR"),
    # 63 TYPEs, numbered in a byte as ET_REC + t: the 64th was Out of memory. TYPE 299 in each role.
    "limit_types": (bas(*[x for i in range(299) for x in (f"type t{i}", " a as integer", "end type")],
                        "type t299", " n as long", " s as string * 4", " v as string", " k() as long", " c as t298",
                        "end type", "declare sub show (r as t299)", "declare function mk (n as long) as t299",
                        "dim r as t299, rs(2) as t299", 'r.n = 299: r.s = "abcd": r.v = "var": r.c.a = 298',
                        "redim r.k(3): r.k(3) = 33", "rs(1) = r", "redim preserve rs(4)", "show rs(1)",
                        "dim q as t299", "q = mk(7)", "print q.n; q.k(2); len(rs(0)) = len(r)", "end",
                        "sub show (r as t299)", " dim l(1) as t299", " l(0) = r",
                        " print l(0).n; l(0).s; l(0).v; l(0).k(3); l(0).c.a", "end sub",
                        "function mk (n as long) as t299", " mk.n = n", " redim mk.k(2): mk.k(2) = n * 2",
                        "end function"),
                    " 299 abcdvar 33  298 \r\n 7  14 -1 \r\n", "ERROR"),
    # 16,384 bindings, line numbers' too: line 16384 was Out of memory.
    "limit_bindings": ("".join(f"{i + 1} n = n + 1\r\n" for i in range(17000)) + "17001 print n\r\n",
                       " 17000 \r\n", "ERROR"),
    # 512 fields, all TYPEs': the 513th was Out of memory.
    "limit_fields": (bas(*[x for i in range(6) for x in (f"type t{i}", *[f" f{j} as integer" for j in range(100)], "end type")],
                         "dim v as t5", "v.f99 = 599", "print v.f99"), " 599 \r\n", "ERROR"),
    # 65,535 variables and names, numbered in 16 bits: the 65,536th was Out of memory.
    "limit_vars": (bas(*[":".join(f"v{i} = {i}" for i in range(k, k + 20)) for k in range(0, 70000, 20)],
                       "print v0; v69999; v35000"), " 0  69999  35000 \r\n", "ERROR"),
    # A procedure's name past 16,383 names carried the array scope's bit: LBOUND of its scalar was 0.
    "limit_scope_bits": (bas(*[":".join(f"w{i} = 1" for i in range(k, k + 20)) for k in range(0, 17000, 20)],
                             "s", "sub s", " dim y as integer", " print lbound(y)", "end sub"),
                         "Array not defined", " 0"),
    # 48 string temporaries, a caller's kept through each call: a string FUNCTION 48 deep was
    # String formula too complex.
    "limit_temps": (bas("function wrap$ (n)", ' if n = 0 then wrap$ = "" else wrap$ = "(" + wrap$(n - 1) + ")"',
                        "end function", "print len(wrap$(1000)); right$(wrap$(1000), 3)"), " 2000 )))\r\n", "ERROR"),
    # A call's parameter count in a byte: past 255 the return left the stack short, 5 + f(..) * 2 598.
    "limit_params": (bas("for i = 1 to 2", f"print 5 + f({', '.join(map(str, range(300)))}) * 2; 9", "next",
                         f"function f ({', '.join(f'p{i}' for i in range(300))})", " f = p0 + p299", "end function"),
                     " 603  9 \r\n 603  9 \r\n", "ERROR"),
    # A frame's cells in 16 bits: 65,540 locals made a 4-cell frame, and the call wrote past the stack.
    # Then a frame past 32 KB was Out of stack space.
    "limit_frame": (bas("s", "sub s", *[":".join(f"v{i} = {i}" for i in range(k, k + 20)) for k in range(0, 65540, 20)],
                        "print v65539", "end sub"), " 65539 \r\n", "ERROR"),
    # The data stack was 32 KB and the frames 800: a FUNCTION 100,000 deep, GOSUB 100,000 deep were
    # Out of stack space.
    "limit_calls": (bas("print f&(100000)", "function f& (n&)", " if n& > 0 then f& = 1 + f&(n& - 1)", "end function"),
                    " 100000 \r\n", "ERROR"),
    "limit_gosubs": (bas("gosub 10: print d&: end", "10 d& = d& + 1", "if d& < 100000 then gosub 10", "return"),
                     " 100000 \r\n", "ERROR"),
    # Calls past the data stack's first segment: by-value strings and a caller's local string live
    # on in the callers' segments, a by-reference argument is the caller's, and ON LOCAL ERROR deep
    # down unwinds to the segment it was set in.
    "limit_call_segments": (bas("print h$(1500, \"\")", "t& = 0: c 3000, t&: print t&", "o", "print f&(5000)",
                                "function h$ (n&, s$)", " dim t$", " t$ = chr$(65 + n& mod 26)",
                                " if n& = 0 then h$ = left$(s$, 3) + right$(s$, 3): exit function",
                                " h$ = h$(n& - 1, (s$ + t$))", "end function",
                                "sub c (n&, t&)", " t& = t& + 1", " if n& > 0 then c n& - 1, t&", "end sub",
                                "sub o", " on local error goto oh", " d 4000", " exit sub", "oh:",
                                " print \"caught\"; err", " resume next", "end sub",
                                "sub d (n&)", " if n& = 0 then error 5 else d n& - 1", "end sub",
                                "function f& (n&)", " if n& > 0 then f& = 1 + f&(n& - 1)", "end function"),
                            "SRQDCB\r\n 3001 \r\ncaught 5 \r\n 5000 \r\n", "ERROR"),
    # A local array on the heap freed at its call's return, the call marked when the array was
    # made: by its own DIM, or by a callee's REDIM of it. Unmarked, 300 calls left 48 MB: Out of
    # memory.
    "local_heap_arrays": (bas("for i = 1 to 300: s 20000: c: next: print \"ok\"",
                              "sub s (n)", " dim b(n): b(n) = 1", "end sub",
                              "sub c", " r a()", "end sub", "sub r (x())", " redim x(20000): x(5) = 1", "end sub"),
                          "ok\r\n", "ERROR"),
    # A SUB's dynamic arrays on its call's arena, not the heap: the newest one ERASEd or REDIMmed
    # gives its room back, REDIM PRESERVE slides into the old one's, its strings' owners following
    # (else z$ moves over them). On the heap FRE dropped 60; an error's recovery keeps the arena.
    "local_arena_arrays": (bas("type t", " s as string", " n as integer", "end type",
                               "s 10", "print \"back\"",
                               "sub s (n)", " f = fre(0)",
                               " for i = 1 to 3000: redim c(n): c(1) = i: erase c: next",
                               " dim a(n)", " for i = 1 to 3000: redim preserve a(n + i mod 7): a(n) = i: next",
                               " print a(n); ubound(a); f - fre(0)",
                               " dim b$(n)", " for j = 0 to n: b$(j) = str$(j): next", " z$ = string$(40, \"z\")",
                               " for i = 1 to 3000: redim preserve b$(n + i mod 5): if i mod 50 = 0 then x = fre(0)",
                               " next", " print b$(0); b$(n); ubound(b$)",
                               " r n", " e n", " k n", "end sub",
                               "sub r (n)", " dim p(n) as t",
                               " for j = 0 to n: p(j).s = \"r\" + str$(j): p(j).n = j: next", " z$ = string$(40, \"z\")",
                               " for i = 1 to 2000: redim preserve p(n + i mod 4) as t: if i mod 50 = 0 then x = fre(0)",
                               " next", " print p(0).s; p(n).s; p(n).n; ubound(p)", "end sub",
                               "sub e (n)", " on local error goto h", " dim a(n): a(1) = 5", " x = 1 / 0",
                               " print a(1); a(n)", " exit sub", "h: resume next", "end sub",
                               "sub k (n)", " redim a(n): a(1) = 7", " g a()", " print a(1); ubound(a)",
                               " redim a(3): a(3) = 2", " print a(3); ubound(a)", "end sub",
                               "sub g (x())", " redim x(20): x(1) = 3", "end sub"),
                           " 3000  14  0 \r\n 0 10 10 \r\nr 0r 10 10  10 \r\n 5  0 \r\n 3  20 \r\n 2  3 \r\n"
                           "back\r\n", "ERROR"),
}


# What the host build (plat/host) cannot show as DOS does, each for its reason.
HOST_SKIP = {"qb_shell": "SHELL runs /bin/sh, whose SET lists its own variables"}


def main() -> None:
    check_forbidden()
    tmp = Path(tempfile.mkdtemp())
    if dosrun.is_host(dosrun.COM):
        check_host_errors(tmp)
        for k in HOST_SKIP:
            CASES.pop(k)
    jobs = {k: v[0] for k, v in (CASES | EXAMPLES).items()}
    batches = [dict(jobs=b, limit_ms=10000, file=True, files=FILES) for b in dosrun.split(jobs)]
    batches += [dict(jobs={k: v[0] for k, v in REPL.items()}, limit_ms=10000),
                dict(jobs={k: v[0] for k, v in WARN.items()}, limit_ms=10000, file=True),
                dict(jobs={k: v[0] for k, v in BIG.items()}, limit_ms=10000, file=True, mem=8)]
    res = dosrun.run_all(batches, tmp)
    every = CASES | REPL | BIG
    bad = [k for k, (_, want, forbid) in every.items() if want not in res[k].out or (forbid and forbidden(forbid, res[k].out))]
    bad += [k for k, (_, want) in WARN.items() if want not in res[k].screen]
    bad += [k for k, (_, want) in EXAMPLES.items() if re.sub(r" +\r\n", "\r\n", res[k].out) != want]
    for k in bad:
        print(f"FAIL {k}: {res[k].reason} {res[k].out!r}")
    sys.exit(1 if bad else print("ok") or 0)


if __name__ == "__main__":
    main()
