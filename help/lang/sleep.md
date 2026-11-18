# SLEEP Statement
keys: SLEEP

Syntax
    SLEEP [seconds]

Pauses the program for seconds. With no argument, or 0, it waits
until a key is pressed. A trapped event (ON TIMER, ON KEY ...) also
ends the wait, once its handler has run.

## Example
    PRINT "Hold on..."
    SLEEP 2
    PRINT "Press a key."
    SLEEP

See also [WAIT](WAIT), [TIMER](TIMER), [INKEY$](INKEY$),
[ON TIMER](ON TIMER)
