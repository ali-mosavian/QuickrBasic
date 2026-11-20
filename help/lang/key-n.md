# KEY(n) Statement
keys: KEY(n)

Syntax
    KEY(n) ON
    KEY(n) OFF
    KEY(n) STOP

Switches trapping of key n, set up with [ON KEY](ON KEY).

See also [KEY](KEY), [ON KEY](ON KEY), [INKEY$](INKEY$)

## Details
    n         key
    1 to 10   F1 to F10
    11 to 14  Up, Left, Right, Down arrows
    15 to 25  keys defined by [KEY](KEY) n, string
    30, 31    F11, F12

ON starts trapping: a press of the key runs the ON KEY handler
instead of reaching the keyboard buffer. STOP holds a press until the
next KEY(n) ON, then the handler runs. OFF stops trapping and the key
is an ordinary key again.

## Example
    ON KEY(1) GOSUB Help
    KEY(1) ON
    DO: LOOP UNTIL INKEY$ = "q"
    END
    Help: PRINT "F1 pressed": RETURN
    ' pressing F1 prints: F1 pressed
