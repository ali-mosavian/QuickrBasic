# UBOUND Function
keys: UBOUND

Syntax
    UBOUND(array [, dimension])

Returns the highest subscript of an array's dimension. dimension
counts from 1 and defaults to 1. A dimension the array lacks is an
Illegal function call.

## Example
    DIM g(1 TO 3, 0 TO 9)
    PRINT UBOUND(g, 1); UBOUND(g, 2)    ' 3  9

See also [LBOUND](LBOUND), [DIM](DIM), [REDIM](REDIM)
