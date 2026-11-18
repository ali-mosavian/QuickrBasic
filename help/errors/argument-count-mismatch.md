# Argument-count mismatch
keys: Argument-count mismatch, error 37

Error 37

A call passes a different number of arguments than the procedure
takes.

See also: [SUB](SUB), [FUNCTION](FUNCTION), [DECLARE](DECLARE)

## Details

Cause: A call gives a different number of arguments than the SUB or
FUNCTION has parameters.

Remedy: Compare the call with the parameter list in the procedure's
heading and the DECLARE.

The DECLARE, the heading and every call must all agree on the count.

## Example
    DECLARE SUB Show (a%, b%)
    CALL Show(1)         ' error 37: needs two arguments
    CALL Show(1, 2)      ' fixed
    SUB Show (a%, b%)
      PRINT a% + b%
    END SUB
