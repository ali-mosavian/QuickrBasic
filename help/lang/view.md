# VIEW Statement
keys: VIEW

Syntax
    VIEW [[SCREEN] (x1, y1)-(x2, y2) [, [fill] [, border]]]

Limits graphics to a rectangle, the viewport. Drawing outside it is
clipped, and coordinates are counted from its top left corner.

  SCREEN   coordinates stay relative to the whole screen; the viewport
           only clips.
  fill     a color attribute that fills the rectangle.
  border   a color attribute for a frame drawn around it.

VIEW with no arguments restores the whole screen. CLS clears only the
viewport.

## Example
    SCREEN 1
    VIEW (20, 20)-(300, 180), , 2
    CIRCLE (100, 80), 200               ' clipped to the frame

See also [VIEW PRINT](VIEW PRINT), [WINDOW](WINDOW), [SCREEN](SCREEN),
[CLS](CLS), [PMAP](PMAP), [SCREEN modes](SCREEN modes)
