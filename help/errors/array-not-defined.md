# Array not defined
keys: Array not defined, Array not dimensioned, error 38

Error 38

## Cause

A name followed by ( is read as an array, but no such array was
dimensioned above this line.

## Remedy

Put `DIM name(upper bound)` above the line. If it is meant to be a
FUNCTION, define or DECLARE it first.

See also: [DIM](DIM), [DECLARE](DECLARE)
