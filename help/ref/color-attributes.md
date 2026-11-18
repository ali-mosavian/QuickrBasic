# Colors
keys: color attributes, colours, attributes, palettes, palette colors

A color is named by an attribute number. What the number shows depends
on the SCREEN mode and the palette.

## The 16 standard colors
Attributes 0 to 15 in text mode and in the 16-color modes:

```
 0 Black         4 Red            8 Gray           12 Light red
 1 Blue          5 Magenta        9 Light blue     13 Light magenta
 2 Green         6 Brown         10 Light green    14 Yellow
 3 Cyan          7 White         11 Light cyan     15 Bright white
```

## Text mode (SCREEN 0)
COLOR foreground, background sets the colors of new text.
- Foreground is 0 to 31: add 16 to a color to make it blink.
- Background is 0 to 7.
- The border is the third argument.

## Graphics modes
```
Mode  Attributes  Notes
1     0-3         background 0-15 and one of two sets:
                  set 0 green, red, brown; set 1 cyan, magenta, white
2     0-1         black and white
7, 8  0-15        the 16 standard colors
9     0-15        the 16 colors, from a palette of 64
10    0-3         shades of gray: off, blinking, normal, bright
11    0-1         two colors, from 262144
12    0-15        the 16 colors, from 262144
13    0-255       256 colors, from 262144
```
In mode 13 the first 16 attributes start as the standard colors.

## Changing a palette
[PALETTE](PALETTE) attribute, color redefines what an attribute shows.
- In modes 9 and 10 the color is a number 0 to 63.
- In modes 11, 12 and 13 it is 65536 * blue + 256 * green + red, each
  part 0 to 63.

```
SCREEN 12
PALETTE 4, 65536 * 20 + 256 * 5 + 63
LINE (0, 0)-(100, 100), 4
```
PALETTE with no arguments puts the default colors back.

## Setting colors
- [COLOR](COLOR) sets the default foreground and background.
- [PSET](PSET), [LINE](LINE), [CIRCLE](CIRCLE) and [PAINT](PAINT) take a
  color argument of their own.
- [POINT](POINT) reads the attribute of one pixel.

See also [SCREEN modes](SCREEN modes), [PALETTE](PALETTE)
