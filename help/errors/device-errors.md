# Device timeout / Device fault / Out of paper
keys: Device timeout, Device fault, Out of paper, Device unavailable, error 24, error 25, error 27, error 68

Errors 24, 25, 27 and 68

A device did not answer.

See also: [LPRINT](LPRINT), [OPEN](OPEN), [ON ERROR](ON ERROR)

## Details

Cause: A device did not answer: the printer is off, out of paper or
not connected, or the device named in OPEN does not exist.

Remedy: Check that the device is ready, then try again. Trap the error
with [ON ERROR](ON ERROR) if the program should go on without it.

Error 24 Device timeout, 25 Device fault, 27 Out of paper, 68 Device
unavailable.

Trap it with ON ERROR; ERR returns the number.

## Example
    ON ERROR GOTO h
    LPRINT "x"           ' printer off: error 25 or 27
    END
    h:
    PRINT "printer error"; ERR
    RESUME NEXT
