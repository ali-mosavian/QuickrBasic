# TIMER Function
keys: TIMER

Syntax
    TIMER

Returns the seconds since midnight as a SINGLE, in hundredths.

See also [TIMER event](TIMER event), [TIME$](TIME$), [SLEEP](SLEEP),
[RANDOMIZE](RANDOMIZE)

## Details

Returns the seconds since midnight as a SINGLE, in hundredths. It
restarts at 0 at midnight, so a long measurement should allow for
the wrap.

To time a short action, take TIMER before and after and subtract. If
the result is negative, midnight passed; add 86400.

## Example
    t! = TIMER
    SLEEP 1
    PRINT TIMER - t!                ' about 1
