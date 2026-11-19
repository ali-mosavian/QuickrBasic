# SWAP Statement
keys: SWAP

Syntax
    SWAP var1, var2

Exchanges the values of two variables, array elements or record
elements.

See also [LET](LET), [LSET](LSET)

## Details

Exchanges the values of two variables, array elements or record
elements. Both must have the same type; a mismatch is a Type
mismatch error.

The two must have the same type exactly: two INTEGERs, two strings,
two records of one TYPE. Mixing INTEGER and LONG is a Type mismatch.
SWAP saves a temporary, as in a sort.

## Example
    a$ = "left": b$ = "right"
    SWAP a$, b$
    PRINT a$; " "; b$                   ' right left
