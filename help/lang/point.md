# POINT Function
keys: POINT

Syntax
    POINT(x, y)
    POINT(n)

POINT(x, y) returns the color attribute of the pixel at (x, y), or -1
if it lies outside the screen or VIEW.

POINT(n) returns the graphics cursor:

  n   result
  0   physical x of the last point drawn
  1   physical y of the last point drawn
  2   logical x (as set by WINDOW)
  3   logical y

Needs a graphics [SCREEN](SCREEN) mode.

## Example
    SCREEN 13
    PSET (10, 10), 40
    IF POINT(10, 10) = 40 THEN PRINT "set"

See also [PSET](PSET), [PMAP](PMAP), [WINDOW](WINDOW)
