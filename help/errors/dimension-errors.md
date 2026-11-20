# Dimension errors
keys: Dynamic array illegal, Lower bound exceeds upper bound, Too many dimensions, Wrong number of dimensions, error 237

An array's bounds or subscripts are invalid.

See also: [DIM](DIM), [REDIM](REDIM), [CONST](CONST)

## Details

Cause: An array has at most 60 dimensions. More bounds in a DIM or
REDIM, or more subscripts in an element, is Too many dimensions,
error 237, when the program is checked.

The other mistakes show when the line runs: a lower bound above its
upper bound is an [Illegal function call](Illegal function call), and
an element with a different number of subscripts than its array has
is [Subscript out of range](Subscript out of range).

Remedy: Use at most 60 dimensions, bounds in the order low TO high,
and as many subscripts as the DIM has.

## Example
    DIM a%(5 TO 1)      ' Illegal function call
    DIM b%(1 TO 5, 3)
    PRINT b%(1)          ' Subscript out of range: two needed
    PRINT b%(1, 1)       ' fixed
