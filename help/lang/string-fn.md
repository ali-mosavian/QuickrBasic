# STRING$ Function
keys: STRING$

Syntax
    STRING$(n, code)
    STRING$(n, text$)

Returns n copies of one character: the one with ASCII code code, or
the first character of text$. n of 0 gives an empty string; a
negative n, or a code outside 0 to 255, is an Illegal function call.

## Example
    PRINT STRING$(5, "-"); STRING$(3, 42)   ' -----***

See also [SPACE$](SPACE$), [CHR$](CHR$), [ASCII](ASCII)
