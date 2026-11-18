# Subscript out of range
keys: Subscript out of range, error 9

Error 9

## Cause

An array subscript is below the lower bound or above the upper bound,
the array has been erased, or a dimension number given to LBOUND or
UBOUND does not exist.

## Remedy

Check the bounds in [DIM](DIM) or [REDIM](REDIM). A loop to
`UBOUND(a)` stays inside.

## Example
    DIM a%(1 TO 3)
    PRINT a%(4)      ' error 9

See also: [DIM](DIM), [UBOUND](UBOUND), [LBOUND](LBOUND)
