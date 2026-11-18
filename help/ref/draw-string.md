# DRAW String Commands
keys: DRAW string, graphics macro language, drawing commands

A string given to [DRAW](DRAW) is a list of commands that move a
pen and draw lines. Spaces are ignored; case does not matter. n is a
number of units, and a unit is one pixel at the default scale.

## Moving and drawing
```
U n   D n   L n   R n     up, down, left, right n units
E n   F n   G n   H n     diagonally: up-right, down-right,
                          down-left, up-left, n units each way
M x,y                     move to point x,y
M +x,-y                   with a sign: move relative to the pen
```
Without n a move is one unit. Each command draws a line as it goes,
unless it is changed by one of the prefixes below.

## Prefixes
```
B         the next move goes without drawing
N         the next move draws, then returns the pen to where it began
```
They go before a move: `B M 100,50`, `N U 20`.

## Color, size and angle
```
C n       drawing color (an attribute for the SCREEN mode)
S n       scale: n/4 pixels a unit, n from 1 to 255 (default 4)
A n       rotate by n quarter turns: 0, 1, 2 or 3 (90 degrees each)
TA n      rotate by n degrees, -360 to 360; counter-clockwise if +
```
`TA` turns by any angle; `A` only by right angles.

## Fill and subpicture
```
P paint,border    fill the shape around the pen with paint,
                  stopping at the color border
X string$         run another DRAW string
```
P needs a closed shape. A numeric variable can stand for n, passed
through VARPTR$ as in PLAY: `"R=" + VARPTR$(w%)`. `R=w%;` also works.

## Example
A box in color 12 with a diagonal, from the point (100,100):
```
SCREEN 12
PSET (100, 100)
DRAW "C12 R40 D40 L40 U40 F40"
```

DRAW needs a graphics SCREEN mode: with SCREEN 0 it is an
[Illegal function call](Illegal function call).

See also [DRAW](DRAW), [Color attributes](color attributes), [SCREEN modes](SCREEN modes)
