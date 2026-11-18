# ON event GOSUB (event trapping)
keys: ON event, ON KEY, ON PLAY, ON PEN, event trapping

Syntax
    ON TIMER(seconds) GOSUB target
    ON KEY(n) GOSUB target
    ON STRIG(n) GOSUB target
    ON PLAY(notes) GOSUB target
    ON PEN GOSUB target

Names a subroutine to call when an event occurs, while the program
runs on. Naming it does nothing yet: the event must be switched on
with its own statement.

  event        switch                        happens
  TIMER(s)     TIMER ON / OFF / STOP         every s seconds
  KEY(n)       KEY(n) ON / OFF / STOP        key n is pressed
  STRIG(n)     STRIG(n) ON / OFF / STOP      a joystick button
  PLAY(n)      PLAY ON / OFF / STOP          music queue drops below n
  PEN          PEN ON / OFF / STOP           never fires: poll PEN(n)

KEY(n): 1-10 are F1-F10, 11-14 the cursor keys (up, left, right,
down), 15-25 keys set with KEY n, string. ON PEN is accepted, but
nothing raises it: read the mouse with PEN(n).

Events are checked between statements. While a handler runs, its own
event is held (as STOP) and fires once after RETURN. Use ON/OFF/STOP
to control this; see [ON and OFF](ON).

## Example
    ON TIMER(1) GOSUB Tick
    TIMER ON
    DO: LOOP UNTIL INKEY$ <> ""
    END
    Tick: PRINT TIME$: RETURN

See also [TIMER](TIMER event), [KEY](KEY), [PLAY](PLAY), [PEN](PEN),
[RETURN](RETURN)
