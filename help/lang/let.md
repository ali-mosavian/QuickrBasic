# LET Statement
keys: LET

Syntax
    [LET] variable = expression

Assigns the value of expression to a variable, array element or
record field. The word LET is optional.

See also [DIM](DIM), [CONST](CONST), [expressions](expressions)

## Details
The value is converted to the variable's type. A string cannot be put
in a numeric variable or the reverse: that is a Type mismatch. A
fraction assigned to a whole-number variable is rounded.

LET is never needed; a plain a = 5 does the same.

## Example
    LET a = 5
    b$ = "total: " + STR$(a)
    arr(2) = a * 2
    PRINT b$                ' total:  5
