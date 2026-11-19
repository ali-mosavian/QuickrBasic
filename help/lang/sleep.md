# SLEEP Statement
keys: SLEEP

Syntax
    SLEEP [seconds]

Pauses the program for seconds.

See also [WAIT](WAIT), [TIMER](TIMER), [INKEY$](INKEY$),
[ON TIMER](ON TIMER)

## Details

Pauses the program for seconds. With no argument, or 0, it waits
until a key is pressed. A trapped event (ON TIMER, ON KEY ...) also
ends the wait, once its handler has run.

For a pause shorter than a second, loop on TIMER. A keystroke ends the
wait but stays in the keyboard buffer: INKEY$ still returns it.

## Example
    PRINT "Hold on..."
    SLEEP 2                        ' waits two seconds
    PRINT "Press a key."
    SLEEP                          ' waits for a key
