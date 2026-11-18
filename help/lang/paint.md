# PAINT Statement
keys: PAINT

Syntax
    PAINT [STEP] (x, y)[, [paint][, [border][, background$]]]

Fills the area around (x, y) with a color or a pattern.

  paint        a color attribute, or a tile string (see below).
               Default: the foreground color.
  border       the attribute of the line that stops the fill. Needed
               when paint is a color or a tile in screen modes with
               more than one color; the fill spreads until it hits it.
  background$  the tile pattern already inside the area, so a fill by
               pattern can tell where to stop. Rarely needed.

STEP makes (x, y) relative to the last point drawn.

A tile is a string; each character is one row of the pattern, 8 pixels
wide in a 2-color mode, 4 pixels wide in a 4-color mode.

The area must be closed. A gap lets the paint leak out to the whole
screen. A start point on the border color paints nothing.

Needs a graphics [SCREEN](SCREEN) mode.

## Example
    SCREEN 1
    CIRCLE (160, 100), 50, 2
    PAINT (160, 100), 3, 2

See also [CIRCLE](CIRCLE), [LINE](LINE), [COLOR](COLOR),
[SCREEN modes](SCREEN modes)
