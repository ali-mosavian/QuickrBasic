# LINE Statement
keys: LINE

Syntax
    LINE [[STEP](x1, y1)]-[STEP](x2, y2) [, [color] [, [B | BF]
         [, style]]]

(To read text, see [LINE INPUT](INPUT).)

Draws a line from (x1, y1) to (x2, y2) in a graphics SCREEN mode; in
text mode it is an Illegal function call.

    omitted (x1, y1)  start at the last point drawn
    STEP              the point is an offset from the last point
    color             attribute, default the foreground color
    B                 draw a box with these two corners
    BF                the same box, filled with color
    style             16-bit pattern, a bit per pixel: &HAAAA is
                      dotted, &HFF00 dashed

Points outside the window are clipped. The last point becomes the
point of the next STEP.

## Example
    SCREEN 12
    LINE (10, 10)-(200, 100), 14
    LINE (50, 50)-(150, 90), 9, B, &HF0F0
    LINE -STEP(40, 0), 12

See also [PSET](PSET), [CIRCLE](CIRCLE), [VIEW](VIEW), [WINDOW](WINDOW),
[SCREEN modes](SCREEN modes)
