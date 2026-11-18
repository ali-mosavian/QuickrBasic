# SCREEN Function
keys: SCREEN function

Syntax
    SCREEN(row, column [, colorflag])

Reads one character cell of the text screen.

See also [SCREEN](SCREEN), [POINT](POINT), [LOCATE](LOCATE),
[COLOR](COLOR)

## Details

Reads one character cell of the text screen.

  row, column   the cell, counted from 1 at the top left.
  colorflag     omitted or 0: the character's ASCII code.
                nonzero: the cell's color attribute, that is
                foreground + 16 * background (+ 128 if blinking).

A cell outside the screen is an Illegal function call. In graphics
modes it reads text printed with PRINT, not drawn pixels.

The attribute splits as: attribute AND 15 is the foreground,
(attribute AND 112) / 16 the background, and attribute AND 128 the
blink bit.

## Example
    CLS
    COLOR 14, 1
    PRINT "A"
    PRINT SCREEN(1, 1); SCREEN(1, 1, 1)     ' 65  30
