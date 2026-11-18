# Array already dimensioned
keys: Array already dimensioned, error 242

Error 242

## Cause

[DIM](DIM) names an array that has been dimensioned, or OPTION BASE
comes after one.

## Remedy

Dimension it once; use [REDIM](REDIM) to resize a dynamic array. Put
OPTION BASE first.

See also: [DIM](DIM), [REDIM](REDIM), [OPTION](OPTION)
