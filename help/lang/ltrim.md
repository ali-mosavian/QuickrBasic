# LTRIM$ Function
keys: LTRIM$

Syntax
    LTRIM$(string)

Returns string without its leading spaces.

See also [RTRIM$](RTRIM$), [STR$](STR$), [LCASE$](LCASE$)

## Details
Other characters, and spaces after the first character, stay. Handy
with STR$, which puts a space before a positive number.

## Example
    PRINT "[" + LTRIM$("   hi ") + "]"      ' [hi ]
    PRINT LTRIM$(STR$(7))                   ' 7
