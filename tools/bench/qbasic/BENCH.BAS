DECLARE SUB Nop (x%)
DECLARE FUNCTION Fib% (n%)
OPEN "R.TXT" FOR OUTPUT AS #1
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 30000: NEXT: NEXT r%: PRINT #1, "for_int"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: a% = 0: FOR i% = 1 TO 10000: a% = (a% + i%) MOD 1000: NEXT: NEXT r%: PRINT #1, "int_math"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: x! = 1: FOR i% = 1 TO 5000: x! = x! * 1.0001 + .5: NEXT: NEXT r%: PRINT #1, "sng_math"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: y# = 0: FOR i% = 1 TO 1000: y# = y# + SQR(i%) * SIN(i%): NEXT: NEXT r%: PRINT #1, "dbl_fn"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: FOR k% = 1 TO 100: s$ = "": FOR i% = 1 TO 50: s$ = s$ + CHR$(65 + i% MOD 26): NEXT: n% = LEN(MID$(s$, 10, 5) + LEFT$(s$, 3)): NEXT: NEXT r%: PRINT #1, "strings"; TIMER - t!
DIM a(500)
t! = TIMER: FOR r% = 1 TO 10: FOR k% = 1 TO 20: FOR i% = 0 TO 500: a(i%) = a(i%) + i%: NEXT: NEXT: NEXT r%: PRINT #1, "arrays"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 5000: Nop i%: GOSUB g: NEXT: NEXT r%: PRINT #1, "calls"; TIMER - t!
DIM f%(8190)
t! = TIMER: FOR r% = 1 TO 10: c% = 0: FOR i% = 0 TO 8190: f%(i%) = 1: NEXT: FOR i% = 0 TO 8190: IF f%(i%) THEN p% = i% + i% + 3: c% = c% + 1: FOR k% = i% + p% TO 8190 STEP p%: f%(k%) = 0: NEXT
NEXT: NEXT r%: PRINT #1, "sieve"; TIMER - t!
DIM v%(300)
t! = TIMER: FOR r% = 1 TO 3: FOR i% = 1 TO 300: v%(i%) = (i% * 37) MOD 1000: NEXT: FOR i% = 1 TO 299: FOR j% = 1 TO 300 - i%: IF v%(j%) > v%(j% + 1) THEN SWAP v%(j%), v%(j% + 1)
NEXT: NEXT: NEXT r%: PRINT #1, "sort"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 5: fb% = Fib%(18): NEXT r%: PRINT #1, "fib"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 2: m& = 0: FOR py% = 0 TO 19: FOR px% = 0 TO 39: cr! = px% / 13 - 2: ci! = py% / 9 - 1.1: zr! = 0: zi! = 0: n% = 0
DO WHILE n% < 50 AND zr! * zr! + zi! * zi! < 4: tz! = zr! * zr! - zi! * zi! + cr!: zi! = 2 * zr! * zi! + ci!: zr! = tz!: n% = n% + 1: LOOP: m& = m& + n%: NEXT: NEXT: NEXT r%: PRINT #1, "mandel"; TIMER - t!
w$ = "the quick brown fox jumps over the lazy dog and keeps on running far away "
t! = TIMER: FOR r% = 1 TO 200: wc% = 0: q% = 1: DO: e% = INSTR(q%, w$, " "): IF e% = 0 THEN EXIT DO
u$ = UCASE$(MID$(w$, q%, e% - q%)): wc% = wc% + 1: q% = e% + 1: LOOP: NEXT r%: PRINT #1, "words"; TIMER - t!
CLS
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 500: LOCATE 1 + i% MOD 24, 1: PRINT "Line"; i%; "of output text here"; : NEXT: NEXT r%: PRINT #1, "print"; TIMER - t!
SCREEN 13
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 20000: PSET (i% MOD 320, i% \ 320 MOD 200), i% MOD 256: NEXT: NEXT r%: PRINT #1, "pset"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 500: LINE (0, i% MOD 200)-(319, 199 - i% MOD 200), i% MOD 256: NEXT: NEXT r%: PRINT #1, "line"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 200: CIRCLE (160, 100), 10 + i% MOD 90, i% MOD 256: NEXT: NEXT r%: PRINT #1, "circle"; TIMER - t!
SCREEN 0
PRINT #1, "check"; a%; x!; y#; n%; a(500); i%; LEN(s$); c%; v%(1); fb%; m&; wc%
CLOSE #1
SYSTEM
g: RETURN
SUB Nop (x%)
END SUB
FUNCTION Fib% (n%)
IF n% < 2 THEN Fib% = n% ELSE Fib% = Fib%(n% - 1) + Fib%(n% - 2)
END FUNCTION
