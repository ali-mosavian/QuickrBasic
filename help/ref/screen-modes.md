# SCREEN Modes
keys: SCREEN modes, screen mode, video modes, resolution, text mode, graphics mode

The mode chosen by [SCREEN](SCREEN) sets the resolution, the number
of colors and the number of text rows and columns.

```
Mode  Kind      Pixels    Text    Colors             Palette
0     text      -         80x25   16 + blink         16 of 64
                          80x43, 80x50 with WIDTH
1     graphics  320x200   40x25   4  of 16           4 sets
2     graphics  640x200   80x25   2                  black, white
7     graphics  320x200   40x25   16                 16
8     graphics  640x200   80x25   16                 16
9     graphics  640x350   80x25   16 of 64           64
10    graphics  640x350   80x25   4 shades           9 shades
11    graphics  640x480   80x30   2 of 262144        262144
12    graphics  640x480   80x30   16 of 262144       262144
13    graphics  320x200   40x25   256 of 262144      262144
```
Rows and columns of text are set with [WIDTH](WIDTH).

See also [SCREEN](SCREEN), [Color attributes](color attributes), [COLOR](COLOR),
[PALETTE](PALETTE), [VIEW](VIEW), [Video pages](video pages)

## Details
What changes between modes
- How many colors an attribute can name: see [Color attributes](color attributes).
- Where the graphics cursor and [coordinates](WINDOW) are: (0,0) is
  the top left; the right and bottom edges are one less than the width
  and height.
- How many [video pages](video pages) exist: these work in every mode.
- Which statements apply: PSET, LINE, CIRCLE, PAINT, DRAW, GET and PUT
  need a graphics mode.

Choosing
- Text programs: mode 0.
- Games with many colors: 13 (320x200, 256 colors).
- Plots and tools: 12 (640x480, 16 colors).
- Old CGA programs: 1 or 2.

Example
```
SCREEN 13
CIRCLE (160, 100), 50, 40
```

## Example
    SCREEN 13
    CIRCLE (160, 100), 50, 40
    ' a circle in color 40 at the center of the 320x200 screen
