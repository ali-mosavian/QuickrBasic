# RETURN without GOSUB
keys: RETURN without GOSUB, error 3

Error 3

RETURN ran with no GOSUB waiting.

See also: [GOSUB](GOSUB), [RETURN](RETURN), [END](END)

## Details

Cause: [RETURN](RETURN) ran with no GOSUB waiting for it, usually
because execution fell through into a subroutine.

Remedy: End the main program with END or GOTO before the subroutines.
Use RETURN only inside a routine entered by [GOSUB](GOSUB).

Usually execution fell through into the subroutine.

Trap it with ON ERROR; ERR returns the number.

## Example
    GOSUB p
    PRINT "back"
    END                  ' fixed: stops before p
    p:
    PRINT "in p"
    RETURN
