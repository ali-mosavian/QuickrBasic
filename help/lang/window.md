# WINDOW Statement
keys: WINDOW

Syntax
    WINDOW [[SCREEN] (x1, y1)-(x2, y2)]

Gives the viewport your own coordinates: (x1, y1) names its lower
left corner and (x2, y2) its upper right, so y grows upward as in a
chart. With SCREEN, y grows downward as on screen.

See also [VIEW](VIEW), [PMAP](PMAP), [SCREEN](SCREEN), [POINT](POINT)

## Details

Gives the viewport your own coordinates: (x1, y1) names its lower
left corner and (x2, y2) its upper right, so y grows upward as in a
chart. With SCREEN, y grows downward as on screen.

The numbers are SINGLE and may be fractions or negative. WINDOW with
no arguments goes back to pixel coordinates. PMAP converts between
the two systems.

A window set with WINDOW makes POINT, PSET, LINE and CIRCLE use the new
coordinates. A CIRCLE radius is then taken in x units. Use VIEW to
choose where on the screen the picture goes.

## Example
    SCREEN 1
    WINDOW (-10, -10)-(10, 10)
    LINE (-10, 0)-(10, 0)               ' the x axis
    LINE (0, -10)-(0, 10)               ' the y axis
