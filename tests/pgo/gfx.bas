' PGO training (tools/x86pgo.py): the graphics, sound and screen statements, which the conformance
' programs leave out
DIM sprite%(200)
FOR m = 1 TO 4
    SELECT CASE m
        CASE 1: SCREEN 13
        CASE 2: SCREEN 12
        CASE 3: SCREEN 9
        CASE 4: SCREEN 1
    END SELECT
    FOR i = 0 TO 30
        PSET (i, i), i MOD 4 + 1: PRESET (i + 1, i)
        LINE (0, i)-(60, i + 5), 2
        LINE (70, 10)-(90, 30), 3, B
        LINE (100, 10)-(120, 30), 1, BF
        CIRCLE (160, 60), 10 + i \ 3, 3
        CIRCLE (160, 60), 15, 2, 0, 3.14, .5
    NEXT
    PAINT (160, 60), 1, 3
    DRAW "BM200,100 C2 R10 D10 L10 U10 TA45 R5"
    x = POINT(5, 5): c = SCREEN(1, 1)
    GET (0, 0)-(15, 15), sprite%
    PUT (40, 40), sprite%, XOR
    PUT (40, 40), sprite%, PSET
    VIEW (10, 10)-(100, 100), 0, 1: WINDOW (0, 0)-(10, 10): PSET (5, 5), 2: VIEW: WINDOW
    PALETTE 1, 2: PALETTE 1, 1
    LOCATE 2, 2: PRINT "text"; m; CSRLIN; POS(0)
    COLOR 2: PRINT USING "##.##"; 3.14159
NEXT
SCREEN 0: WIDTH 80: COLOR 7, 0: CLS
FOR i = 1 TO 20: LOCATE i, i: PRINT STRING$(i, "*"); : NEXT
SOUND 440, .1: PLAY "MB T200 O3 L16 CDEFG": BEEP
PRINT "done"
