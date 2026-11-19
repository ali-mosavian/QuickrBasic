# Duplicate definition
keys: Duplicate definition, error 10, ~Duplicate definition /function

Error 10

A name is declared twice with different meanings.

See also: [DIM](DIM), [CONST](CONST), [REDIM](REDIM)

## Details

Cause: A name is declared twice with different meanings: an array
dimensioned again, a CONST also used as a variable, a variable with
the name of a FUNCTION that takes arguments, or a second definition of
a procedure.

Remedy: Give each thing its own name. Dimension an array once; use
[REDIM](REDIM) to resize a dynamic one.

Causes: an array dimensioned twice, a CONST also used as a variable, a
variable named like a FUNCTION, a procedure defined twice.

## Example
    CONST max = 5
    DIM max AS INTEGER   ' error 10
    DIM top AS INTEGER   ' fixed: own name
