# Dimension errors
keys: Dynamic array illegal, Lower bound exceeds upper bound, Too many dimensions, Wrong number of dimensions

An array's bounds or subscripts are invalid.

See also: [DIM](DIM), [REDIM](REDIM), [CONST](CONST)

## Details

Cause: A fixed-size array's bounds are not constants, a lower bound is
above its upper bound, an array has more than 60 dimensions, or it is
indexed with a different number of subscripts than it has.

Remedy: Use numbers or [CONST](CONST) for fixed bounds, in the order
low TO high. Match the subscripts to the DIM.

A lower bound may not exceed its upper bound; at most 60 dimensions; a
subscript list must match the DIM.

## Example
    DIM a%(5 TO 1)      ' error: lower above upper
    DIM a%(1 TO 5, 3)
    PRINT a%(1)          ' error: two subscripts needed
    PRINT a%(1, 1)       ' fixed
