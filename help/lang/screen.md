# SCREEN Statement
keys: SCREEN

Syntax
    SCREEN mode [, [colorswitch] [, [apage] [, vpage]]]

Sets the screen mode and clears the screen.

See also [SCREEN function](SCREEN function), [SCREEN modes](SCREEN modes),
[video pages](video pages), [PCOPY](PCOPY), [WIDTH](WIDTH),
[VIEW](VIEW), [WINDOW](WINDOW)

## Details

Sets the screen mode and clears the screen.

  mode          the mode number. See [SCREEN modes](SCREEN modes).
  colorswitch   modes 0 and 1 only. In mode 0, 0 shows text without
                color and nonzero with color; in mode 1 the meanings
                are the other way round.
  apage         the page that output draws on.
  vpage         the page you see. Both default to 0.

Video pages work in every mode, not only the text ones QBasic allowed;
see [video pages](video pages).

Giving a mode the adapter lacks, or a page it lacks, is an Illegal
function call. SCREEN with the mode already set still clears it.

Mode numbers are QBasic's. The color switch is rarely needed now;
leave it out. Changing the mode resets the palette, the viewport (VIEW),
the window (WINDOW) and the color to the mode's defaults.

## Example
    SCREEN 12
    CIRCLE (320, 240), 100, 14
