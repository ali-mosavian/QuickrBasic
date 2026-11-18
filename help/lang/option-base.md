# OPTION BASE Statement
keys: OPTION BASE, OPTION

Syntax
    OPTION BASE {0 | 1}

Sets the lowest subscript of arrays declared without TO. The default
is 0, so DIM a(5) has six elements, a(0) to a(5). After OPTION BASE 1
it has five, a(1) to a(5).

It may appear once, and must come before any array is used or
declared in the program. A second one, or one after an array, is
"Duplicate definition". An array declared with TO, as in
DIM a(3 TO 8), ignores it.

## Example
    OPTION BASE 1
    DIM a(3)
    PRINT LBOUND(a), UBOUND(a)     ' 1   3

See also [DIM](DIM), [LBOUND](LBOUND), [UBOUND](UBOUND)
