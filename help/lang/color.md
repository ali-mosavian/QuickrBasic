# COLOR Statement
keys: COLOR

Syntax
    COLOR [foreground] [, [background] [, border]]    SCREEN 0
    COLOR [background] [, palette]                    SCREEN 1
    COLOR [foreground]                                SCREEN 4, 12, 13
    COLOR [foreground] [, background]                 SCREEN 7 to 10

Sets the colors PRINT and the drawing statements use afterwards.
Leave a part out to keep it.

See also [PALETTE](PALETTE), [CLS](CLS), [SCREEN modes](SCREEN modes),
[SCREEN](SCREEN)

## Details
Sets the colors PRINT and the drawing statements use afterwards.
Leave a part out to keep it.

In SCREEN 0, foreground is 0 to 31 (16 to 31 blink), background 0
to 7, border 0 to 15. In SCREEN 1, background is 0 to 15 and
palette is 0 (green, red, brown) or 1 (cyan, magenta, white), which
sets what attributes 1 to 3 look like. In the other modes,
foreground is an attribute valid in that mode, and background in
SCREEN 7 to 10 is a color value.

A second argument that a mode does not take, or a value out of
range, gives Illegal function call. Use PALETTE to change what an
attribute looks like.

## Example
    COLOR 14, 1        ' yellow on blue
    CLS
    PRINT "Hello"
