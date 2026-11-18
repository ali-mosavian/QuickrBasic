# DRAW Statement
keys: DRAW

Syntax
    DRAW commandstring$

DRAW draws and moves a pen on the graphics screen by a small
language held in a string: movement (U, D, L, R, E, F, G, H, M),
the pen (B for blank moves, N for return to start), colour C, scale
S, angle A and TA, fills P, and X to run another string.

The pen starts at the last point drawn. Numbers inside the string
may be given as a variable with the form =name; for example
"R=len;". Needs a graphics SCREEN mode, otherwise Illegal function
call.

The full command list is under [DRAW string](DRAW string).

## Example
    SCREEN 1
    side = 40
    DRAW "BM100,60 R=side; D=side; L=side; U=side;"

See also [DRAW string](DRAW string), [PSET](PSET), [LINE](LINE),
[SCREEN modes](SCREEN modes)
