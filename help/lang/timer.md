# TIMER Function
keys: TIMER

Syntax
    TIMER

Returns the seconds since midnight as a SINGLE, in hundredths. It
restarts at 0 at midnight, so a long measurement should allow for
the wrap.

## Example
    t! = TIMER
    SLEEP 1
    PRINT TIMER - t!                    ' about 1

See also [TIMER event](TIMER event), [TIME$](TIME$), [SLEEP](SLEEP),
[RANDOMIZE](RANDOMIZE)
