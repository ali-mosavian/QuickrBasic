# PEN Statement and Function
keys: PEN, PEN(n), PEN ON, PEN OFF, PEN STOP, light pen

Syntax
    PEN(n)
    PEN ON | OFF | STOP

QuickrBasic has no light pen: PEN reads the mouse, the left button
being the pen's switch.

See also [QuickrBasic](QuickrBasic), [SCREEN](SCREEN)

## Details
This gives programs a mouse without CALL INTERRUPT or a driver.

PEN(n)
n from 0 to 9 reads the left button; n + 10 gives the same reading for
the right button (10 to 19). Any other n is Illegal function call.

    n   Returns
    0   -1 if the button was pressed since the last PEN(0), else 0
    1   x of the last press, in pixels
    2   y of the last press
    3   -1 while the button is down now, else 0
    4   x of the mouse now
    5   y of the mouse now
    6   row of the last press (text cells, from 1)
    7   column of the last press
    8   row of the mouse now
    9   column of the mouse now

PEN(0) and PEN(10) are latched: a click between two calls is not lost,
and reading it clears it. Clicks made in the editor before RUN do not
count. With no mouse every call returns 0, except the row and column
readings, which return 1.

PEN ON, OFF and STOP are accepted, but no pen event ever fires, so
[ON PEN](ON event) never calls its handler. Poll PEN(0) instead.

## Example
    SCREEN 12
    DO
      IF PEN(0) THEN CIRCLE (PEN(1), PEN(2)), 6, 14
      IF PEN(10) THEN CLS
    LOOP UNTIL INKEY$ <> ""
    ' left click draws a circle, right click clears the screen
