# CIRCLE Statement
keys: CIRCLE

Syntax
    CIRCLE [STEP] (x, y), radius [, [color] [, [start] [, [end]
        [, aspect]]]]

Draws a circle, ellipse or arc in a graphics screen mode.

    (x, y)   centre; with STEP, relative to the graphics cursor
    radius   in the current coordinate system (see WINDOW)
    color    attribute; default is the current foreground color
    start,
    end      arc angles in radians, 0 to 2*PI, 0 being three
             o'clock, counter-clockwise. A negative angle (its sign
             is dropped) also draws the radius line to that end.
    aspect   y length divided by x length. Under 1 the ellipse is
             wide, over 1 tall. The radius is the longer axis.

Without aspect, the circle looks round on the screen's pixel shape.
In SCREEN 0 (text) CIRCLE gives Illegal function call. Parts
outside the screen or VIEW are clipped.

## Example
    SCREEN 12
    CIRCLE (320, 240), 100, 14
    CIRCLE (320, 240), 100, 12, , , .4      ' flat ellipse
    CIRCLE (320, 240), 60, 10, -.5, -1.5    ' wedge

See also [LINE](LINE), [PAINT](PAINT), [DRAW](DRAW), [COLOR](COLOR),
[VIEW](VIEW), [WINDOW](WINDOW), [SCREEN modes](SCREEN modes)
