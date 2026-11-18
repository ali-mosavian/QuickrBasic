# LTRIM$ Function
keys: LTRIM$

Syntax
    LTRIM$(string)

Returns string without its leading spaces. Other characters, and
spaces after the first character, stay.

## Example
    PRINT "[" + LTRIM$("   hi ") + "]"      ' [hi ]
    PRINT LTRIM$(STR$(7))                   ' 7

See also [RTRIM$](RTRIM$), [STR$](STR$), [LCASE$](LCASE$)
