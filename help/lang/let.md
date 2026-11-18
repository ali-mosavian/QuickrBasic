# LET Statement
keys: LET

Syntax
    [LET] variable = expression

Assigns the value of expression to variable, a simple variable, array
element or record field. The word LET is optional and never needed.

The value is converted to the variable's type. A string cannot be put
in a numeric variable or the reverse: that is a Type mismatch.

## Example
    LET a = 5
    b$ = "total: " + STR$(a)
    arr(2) = a * 2

See also [DIM](DIM), [CONST](CONST), [expressions](expressions)
