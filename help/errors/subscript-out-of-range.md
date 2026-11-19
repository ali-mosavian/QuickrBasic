# Subscript out of range
keys: Subscript out of range, error 9

Error 9

An array subscript is outside the array's bounds.

See also: [DIM](DIM), [UBOUND](UBOUND), [LBOUND](LBOUND)

## Details

Cause: An array subscript is below the lower bound or above the upper
bound, the array has been erased, or a dimension number given to
LBOUND or UBOUND does not exist.

Remedy: Check the bounds in [DIM](DIM) or [REDIM](REDIM). A loop to
`UBOUND(a)` stays inside.

Also raised for an erased array, or a LBOUND/UBOUND dimension that
does not exist.

Trap it with ON ERROR; ERR returns the number.

## Example
    DIM a%(1 TO 3)
    PRINT a%(4)          ' error 9
    PRINT a%(UBOUND(a%)) ' fixed: 0
