# STEP Keyword
keys: STEP

Syntax
    FOR v = a TO b STEP increment
    LINE STEP (x, y)-STEP (dx, dy)

In FOR, STEP is the amount added to the counter on each pass. It is
1 if omitted; a negative STEP counts down, and the loop ends when
the counter passes the limit.

See also [FOR](FOR), [LINE](LINE), [CIRCLE](CIRCLE), [PSET](PSET),
[PAINT](PAINT), [GET](GET), [PUT](PUT)

## Details

In FOR, STEP is the amount added to the counter on each pass. It is
1 if omitted; a negative STEP counts down, and the loop ends when
the counter passes the limit.

In graphics statements, STEP before a point makes its coordinates
relative to the last point drawn, not to the screen's corner.

A relative point is the last point plus (dx, dy). After a LINE the last
point is its end; after PSET, the pixel set; after CIRCLE, the center.
STEP 0 in a FOR is a loop that never ends unless left with EXIT FOR.

## Example
    FOR i% = 10 TO 0 STEP -5
        PRINT i%                        ' 10  5  0
    NEXT i%
    SCREEN 1
    PSET (50, 50)
    LINE -STEP (30, 0)
