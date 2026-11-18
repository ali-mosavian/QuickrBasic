# Subprogram not defined
keys: Subprogram not defined, error 35

Error 35

A SUB is CALLed or DECLAREd but never defined.

See also: [SUB](SUB), [CALL](CALL), [DECLARE](DECLARE)

## Details

Cause: A [SUB](SUB) is [CALL](CALL)ed or DECLAREd, but no SUB of that
name exists.

Remedy: Write `SUB name ... END SUB`, or remove the DECLARE and the
calls.

Define it, or remove the DECLARE and the calls.

## Example
    CALL Hello          ' error 35: no SUB Hello
    SUB Hello            ' fixed
      PRINT "hi"
    END SUB
