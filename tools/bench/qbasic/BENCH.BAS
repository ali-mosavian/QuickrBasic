DECLARE SUB Nop (x%)
OPEN "R.TXT" FOR OUTPUT AS #1
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 30000: NEXT: NEXT r%: PRINT #1, "for_int"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: a% = 0: FOR i% = 1 TO 10000: a% = (a% + i%) MOD 1000: NEXT: NEXT r%: PRINT #1, "int_math"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: x! = 1: FOR i% = 1 TO 5000: x! = x! * 1.0001 + .5: NEXT: NEXT r%: PRINT #1, "sng_math"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: y# = 0: FOR i% = 1 TO 1000: y# = y# + SQR(i%) * SIN(i%): NEXT: NEXT r%: PRINT #1, "dbl_fn"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: FOR k% = 1 TO 100: s$ = "": FOR i% = 1 TO 50: s$ = s$ + CHR$(65 + i% MOD 26): NEXT: n% = LEN(MID$(s$, 10, 5) + LEFT$(s$, 3)): NEXT: NEXT r%: PRINT #1, "strings"; TIMER - t!
DIM a(500)
t! = TIMER: FOR r% = 1 TO 10: FOR k% = 1 TO 20: FOR i% = 0 TO 500: a(i%) = a(i%) + i%: NEXT: NEXT: NEXT r%: PRINT #1, "arrays"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 5000: Nop i%: GOSUB g: NEXT: NEXT r%: PRINT #1, "calls"; TIMER - t!
CLS
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 500: LOCATE 1 + i% MOD 24, 1: PRINT "Line"; i%; "of output text here"; : NEXT: NEXT r%: PRINT #1, "print"; TIMER - t!
SCREEN 13
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 20000: PSET (i% MOD 320, i% \ 320 MOD 200), i% MOD 256: NEXT: NEXT r%: PRINT #1, "pset"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 500: LINE (0, i% MOD 200)-(319, 199 - i% MOD 200), i% MOD 256: NEXT: NEXT r%: PRINT #1, "line"; TIMER - t!
t! = TIMER: FOR r% = 1 TO 10: FOR i% = 1 TO 200: CIRCLE (160, 100), 10 + i% MOD 90, i% MOD 256: NEXT: NEXT r%: PRINT #1, "circle"; TIMER - t!
SCREEN 0
PRINT #1, "check"; a%; x!; y#; n%; a(500); i%; LEN(s$)
CLOSE #1
SYSTEM
g: RETURN
SUB Nop (x%)
END SUB
