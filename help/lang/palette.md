# PALETTE Statement
keys: PALETTE, PALETTE USING

Syntax
    PALETTE [attribute, color]
    PALETTE USING array(index)

Changes which on-screen color an attribute number shows.

See also [COLOR](COLOR), [SCREEN modes](SCREEN modes), [OUT](OUT)

## Details
  attribute   the attribute to change (0 up to the mode's maximum)
  color       what to show: with a 16-color palette, 0 to 15; with 256
              colors, a number built as 65536*blue + 256*green + red,
              each part 0 to 63
  array       an INTEGER array: one color per attribute, starting at
              element index. It needs enough elements to cover them all.

PALETTE alone restores the mode's default palette. Pixels on screen
change at once; this is the way to fade or cycle colors without
redrawing. Text already printed in that attribute changes too.

Only modes with an EGA, VGA or MCGA palette take it; in the others it
is "Illegal function call".

## Example
    SCREEN 13
    PALETTE 1, 63                 ' attribute 1: bright red (red=63)
    PALETTE 2, 63 * 256           ' attribute 2: bright green
    PALETTE                       ' back to default
