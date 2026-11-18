# Dimension errors
keys: Dynamic array illegal, Lower bound exceeds upper bound, Too many dimensions, Wrong number of dimensions

## Cause

A fixed-size array's bounds are not constants, a lower bound is above
its upper bound, an array has more than 60 dimensions, or it is
indexed with a different number of subscripts than it has.

## Remedy

Use numbers or [CONST](CONST) for fixed bounds, in the order low TO
high. Match the subscripts to the DIM.

See also: [DIM](DIM), [REDIM](REDIM), [CONST](CONST)
