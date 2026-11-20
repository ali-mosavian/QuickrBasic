# UBOUND Function
keys: UBOUND

Syntax
    UBOUND(array [, dimension])

Returns the highest subscript of an array's dimension.

See also [LBOUND](LBOUND), [DIM](DIM), [REDIM](REDIM)

## Details

Returns the highest subscript of an array's dimension. dimension
counts from 1 and defaults to 1. A dimension the array lacks, or an
array not dimensioned, is Subscript out of range. array may be a
record's array element, UBOUND(v.pts), in QuickrBasic.

An array with no explicit lower bound starts at 0, or at 1 under
OPTION BASE 1. The element count of a dimension is UBOUND - LBOUND + 1.

## Example
    DIM g(1 TO 3, 0 TO 9)
    PRINT UBOUND(g, 1); UBOUND(g, 2)    ' 3  9
