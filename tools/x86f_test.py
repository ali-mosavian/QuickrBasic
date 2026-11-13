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
    # C port: a string literal left the cursor at its text, so the ':' after it was a Syntax error.
    "c_str_colon": ('X$="HI":PRINT X$\r\n', "HI\r\n", "ERROR"),
    # C port: llrm-cc's switch sent END FUNCTION and END DEF to "END SUB/FUNCTION without
    # SUB/FUNCTION" without checking for an open procedure.
    "c_end_function": ("10 DEF FNT$(A$)\r\n20 FNT$ = A$ + \"!\"\r\n30 END DEF\r\n40 PRINT F%(3); FNT$(\"X\")\r\n"
                       "50 FUNCTION F%(A%)\r\n60 F% = A% * 2\r\n70 END FUNCTION\r\nRUN\r\n", " 6 X!\r\n", "ERROR"),
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
                   "e 11 \r\nt\r\nd\r\nDivision by zero", "no"),
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
    "qb_diag_end": ("PRINT a(1\r\n", "  PRINT a(1\r\n           ^\r\nThe PRINT statement is incomplete: "
                    "the line ends where more is needed.\r\nPRINT is written:\r\n  PRINT [#file,]", None),
    "qb_diag_goto": ("ON 1 PRINT\r\n", "\r\n       ^\r\n`PRINT` should be followed by GOTO or GOSUB "
                     "here.\r\nWrite `ON expression GOTO label, label`.", None),
    "qb_diag_tm": ('x = "a" + 1\r\n', '\r\n            ^\r\n`1` has the wrong type for this place (', None),
    # A statement the automaton rejected said only that it does not fit; now what it wanted.
    "qb_diag_exp": ("FOR i = 1 3\r\nNEXT\r\n", "Expected `TO` in the FOR statement, but found `3`.\r\n"
                    "FOR is written:\r\n  FOR counter = start TO end [STEP step]", None),
    "qb_diag_exp2": ('OPEN "x" FOR FOO AS 1\r\n', "Expected `INPUT`, `APPEND`, `BINARY`, `OUTPUT` or "
                     "`RANDOM` in the OPEN statement, but found `FOO`.", None),
    "qb_diag_kw": ('NAME "a" "b"\r\n', 'Expected `AS` in the NAME statement, but found `"b"`.', None),
    # The blocks' errors were a Syntax error, or NEXT without FOR, with no line shown and no words.
    "qb_blk_open": ("PRINT 1\r\nFOR i = 0 TO 9\r\nPRINT i\r\n", "FOR without NEXT in line No line number"
                    " in module", None),
    "qb_blk_open2": ("PRINT 1\r\nFOR i = 0 TO 9\r\nPRINT i\r\n", "  FOR i = 0 TO 9\r\n  ^\r\nThis FOR loop,"
                     " counting with `i`, is never closed", None),
    "qb_blk_nextv": ("FOR i = 1 TO 3\r\nNEXT j\r\n", "  NEXT j\r\n       ^\r\nThis NEXT ends a loop counting "
                     "with `j`, but the loop open here counts with `i`.\r\nWrite `NEXT i`, or just NEXT.", " 1"),
    "qb_blk_cross": ("FOR i = 1 TO 3\r\nDO\r\nNEXT i\r\n", "NEXT without FOR in line No line number in module"
                     "", None),
    "qb_blk_cross2": ("FOR i = 1 TO 3\r\nDO\r\nNEXT i\r\n", "the DO block started inside it is still open",
                      None),
    "qb_blk_loop": ("PRINT 1\r\nLOOP\r\n", "LOOP without DO in line", " 1"),
    # &H and &O took no type suffix: &HFF& was a Syntax error at the &.
    "qb_hex_sfx": ("CONST a% = &H10%, b& = &HFF&: PRINT a%; b&; &O17&; &HFFFF&\r\n", " 16  255  15  65535 \r\n", "ERROR"),
    # A call of a SUB or FUNCTION only DECLAREd was a syntax error; as BC's, it compiles and fails when run.
    "qb_pdecl": ('DECLARE SUB ext (a%, b$)\r\nDECLARE FUNCTION fx% (a%)\r\nON ERROR GOTO h\r\next 1, "x"\r\n'
                 'CALL ext(2, "y")\r\ny = fx%(3) + 1\r\nPRINT "d"\r\nEND\r\nh: PRINT ERR;: RESUME NEXT\r\n',
                 " 35  35  18 d\r\n", "ERROR"),
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
    "qb_blk_next2": ("FOR i = 1 TO 2: FOR j = 1 TO 2: NEXT j, i: PRINT \"ok\"\r\n", "ok", "ERROR"),
    "qb_sng_range": ('PRINT VAL("1.23456789012"); 1E-45; 1 / 1E-45\r\n'
                     'a! = 3.4E+38: d# = 1D+40: x! = 1E-38: PRINT a! * 10; CSNG(d#); x! / 1000\r\n',
                     " 1.23456789012  1.401298E-45  1.#INF \r\n 1.#INF  1.#INF  9.999666E-42 \r\n",
                     "ERROR"),
}

# Run as files with 8 MB: the heap is the memory free.
BIG = {
    # The heap was a fixed 4 MB whatever the machine had: 4.8 MB of arrays were Out of memory.
    "qb_heap": ('DIM a(999, 99) AS DOUBLE, b(999, 99) AS DOUBLE, c(999, 99) AS DOUBLE\r\n'
                'DIM d(999, 99) AS DOUBLE, e(999, 99) AS DOUBLE, f(999, 99) AS DOUBLE\r\n'
                'f(999, 99) = 3: PRINT f(999, 99)\r\n', " 3 \r\n", "ERROR"),
    # Names and variables were fixed tables of 4,096: the 4,097th was Out of memory.
    # Made while a string was in the heap the table could not grow: Out of memory; failing, it crashed.
    "many_vars_heap": ('a$ = "s" + STR$(1)\r\n' + "".join(f"v{i} = {i}\r\n" for i in range(1500)) + "PRINT a$; v1400\r\n",
                       "s 1 1400 \r\n", "ERROR"),
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
}

# Typed at the prompt, not run as a file.
REPL = {
    # A direct line ran from DCODE, its statement map's place: nothing printed.
    "repl_direct": ("PRINT 5\r\n", " 5 \r\n", "ERROR"),
    # A numbered line stepped 3 bytes past its M_INT, which is 5: the line lost its first bytes.
    "repl_numbered": ("10 PRINT 5\r\n20 PRINT 6\r\nRUN\r\n", " 5 \r\n 6 \r\n", "ERROR"),
}


# Companions the cases read, beside them.
FILES = {
    "QB_CH2.BAS": b'COMMON b%, q$, e#, y(), u AS SHORT\r\nPRINT b%; q$; e#; y(3); LBOUND(y); u; t\r\n'
                  b'PRINT #1, "kept": CLOSE #1\r\nON ERROR GOTO h\r\nCHAIN "NONE"\r\nEND\r\nh: PRINT "e"; ERR: RESUME NEXT\r\n',
}

# A forbidden "ERROR": any error's message at a line's start.
MSGS = x86run.messages()


def forbidden(forbid: str, out: str) -> bool:
    lines = "\n" + out.replace("\r", "")
    return any("\n" + m in lines for m in MSGS) if forbid == "ERROR" else forbid in out


def main() -> None:
    tmp = Path(tempfile.mkdtemp())
    batches = [dict(jobs=b, limit_ms=10000, file=True, files=FILES) for b in x86run.split({k: v[0] for k, v in CASES.items()})]
    batches += [dict(jobs={k: v[0] for k, v in REPL.items()}, limit_ms=10000),
                dict(jobs={k: v[0] for k, v in WARN.items()}, limit_ms=10000, file=True),
                dict(jobs={k: v[0] for k, v in BIG.items()}, limit_ms=10000, file=True, mem=8)]
    res = x86run.run_all(batches, tmp)
    every = CASES | REPL | BIG
    bad = [k for k, (_, want, forbid) in every.items() if want not in res[k].out or (forbid and forbidden(forbid, res[k].out))]
    bad += [k for k, (_, want) in WARN.items() if want not in res[k].screen]
    for k in bad:
        print(f"FAIL {k}: {res[k].reason} {res[k].out!r}")
    sys.exit(1 if bad else print("ok") or 0)


if __name__ == "__main__":
    main()
