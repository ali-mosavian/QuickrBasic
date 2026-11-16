FOR r% = 1 TO 100: FOR k% = 1 TO 100: s$ = "": FOR i% = 1 TO 50: s$ = s$ + CHR$(65 + i% MOD 26): NEXT: n% = LEN(MID$(s$, 10, 5) + LEFT$(s$, 3)): NEXT: NEXT r%: PRINT n%; s$
