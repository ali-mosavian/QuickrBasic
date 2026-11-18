# PSET and PRESET Statements
keys: PSET, PRESET

Syntax
    PSET [STEP] (x, y)[, color]
    PRESET [STEP] (x, y)[, color]

Draws one pixel at (x, y). STEP makes the point relative to the last
one drawn.

  PSET     uses color, or the foreground color if left out
  PRESET   uses color, or the background color if left out, so with
           no color it erases the pixel

A point outside the VIEW is silently not drawn. Needs a graphics
[SCREEN](SCREEN) mode.

## Example
    SCREEN 1
    FOR x = 0 TO 99: PSET (x, x), 2: NEXT
    PRESET (50, 50)                ' knock a hole in the line

See also [POINT](POINT), [LINE](LINE), [COLOR](COLOR), [VIEW](VIEW)
