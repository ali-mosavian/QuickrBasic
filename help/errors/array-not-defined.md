# Array not defined
keys: Array not defined, Array not dimensioned, error 38

Error 38

A name followed by ( is read as an array that was never dimensioned.

See also: [DIM](DIM), [DECLARE](DECLARE)

## Details

Cause: A name followed by ( is read as an array, but no such array was
dimensioned above this line.

Remedy: Put `DIM name(upper bound)` above the line. If it is meant to
be a FUNCTION, define or DECLARE it first.

A call to an undefined FUNCTION gives the same message.

## Example
    x% = a%(3)          ' error 38: a% never dimensioned
    DIM a%(10)           ' fix: dimension it above the use
    x% = a%(3)
