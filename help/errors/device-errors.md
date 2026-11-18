# Device timeout / Device fault / Out of paper
keys: Device timeout, Device fault, Out of paper, Device unavailable, error 24, error 25, error 27, error 68

Errors 24, 25, 27 and 68

## Cause

A device did not answer: the printer is off, out of paper or not
connected, or the device named in OPEN does not exist.

## Remedy

Check that the device is ready, then try again. Trap the error with
[ON ERROR](ON ERROR) if the program should go on without it.

See also: [LPRINT](LPRINT), [OPEN](OPEN), [ON ERROR](ON ERROR)
