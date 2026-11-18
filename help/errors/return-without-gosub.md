# RETURN without GOSUB
keys: RETURN without GOSUB, error 3

Error 3

## Cause

[RETURN](RETURN) ran with no GOSUB waiting for it, usually because
execution fell through into a subroutine.

## Remedy

End the main program with END or GOTO before the subroutines. Use
RETURN only inside a routine entered by [GOSUB](GOSUB).

See also: [GOSUB](GOSUB), [RETURN](RETURN), [END](END)
