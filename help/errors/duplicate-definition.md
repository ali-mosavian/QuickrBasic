# Duplicate definition
keys: Duplicate definition, error 10, ~Duplicate definition /function

Error 10

## Cause

A name is declared twice with different meanings: an array dimensioned
again, a CONST also used as a variable, a variable with the name of a
FUNCTION that takes arguments, or a second definition of a procedure.

## Remedy

Give each thing its own name. Dimension an array once; use
[REDIM](REDIM) to resize a dynamic one.

See also: [DIM](DIM), [CONST](CONST), [REDIM](REDIM)
