# LBOUND Function
keys: LBOUND

Syntax
    LBOUND(array [, dimension])

Returns the lowest subscript of an array's dimension.

See also [UBOUND](UBOUND), [DIM](DIM), [OPTION BASE](OPTION BASE)

## Details
dimension runs from 1 to the number of dimensions (default 1). The
result is 0 unless the array was declared with a lower bound or
OPTION BASE 1 is in force.

An array that is not dimensioned, or a dimension out of range, is a
Subscript out of range error.

## Example
    DIM a(5 TO 9, 3)
    PRINT LBOUND(a), LBOUND(a, 2)    ' 5  0
    PRINT UBOUND(a), UBOUND(a, 2)    ' 9  3
