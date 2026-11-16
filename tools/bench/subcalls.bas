DECLARE SUB Nop (x%)
FOR r% = 1 TO 200: FOR i% = 1 TO 5000: Nop i%: GOSUB g: NEXT: NEXT r%: PRINT i%: END
g: RETURN
SUB Nop (x%)
END SUB
