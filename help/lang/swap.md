# SWAP Statement
keys: SWAP

Syntax
    SWAP var1, var2

Exchanges the values of two variables, array elements or record
elements. Both must have the same type; a mismatch is a Type
mismatch error.

## Example
    a$ = "left": b$ = "right"
    SWAP a$, b$
    PRINT a$; " "; b$                   ' right left

See also [LET](LET), [LSET](LSET)
