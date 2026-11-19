# Array already dimensioned
keys: Array already dimensioned, error 242

Error 242

DIM names an array that already exists, or OPTION BASE comes after
one.

See also: [DIM](DIM), [REDIM](REDIM), [OPTION](OPTION)

## Details

Cause: [DIM](DIM) names an array that has been dimensioned, or OPTION
BASE comes after one.

Remedy: Dimension it once; use [REDIM](REDIM) to resize a dynamic
array. Put OPTION BASE first.

A fixed array cannot be dimensioned twice. A dynamic array ($DYNAMIC
or DIM with variable bounds) can be resized with REDIM; ERASE frees
it.

## Example
    DIM a%(10)
    DIM a%(20)           ' error 242
    ' fix: dimension once, or use a dynamic array
    REDIM b%(10)
    REDIM b%(20)
    PRINT UBOUND(b%)     ' 20
