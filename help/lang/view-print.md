# VIEW PRINT Statement
keys: VIEW PRINT

Syntax
    VIEW PRINT [top TO bottom]

Sets the text window: the rows that PRINT writes to and that scroll.
Rows outside it stay untouched. top and bottom are row numbers, from
1, with top at most bottom; otherwise Illegal function call. LOCATE
works only inside the window, and CLS clears only it.

VIEW PRINT alone restores the whole screen.

## Example
    CLS
    LOCATE 1, 1: PRINT "A fixed title"
    VIEW PRINT 3 TO 20
    FOR i% = 1 TO 40: PRINT i%: NEXT i%
    VIEW PRINT

See also [VIEW](VIEW), [CLS](CLS), [LOCATE](LOCATE), [WIDTH](WIDTH),
[PRINT](PRINT)
