# LCASE$ Function
keys: LCASE$

Syntax
    LCASE$(string)

Returns string with every capital letter A to Z made lowercase. Other
characters are unchanged.

## Example
    PRINT LCASE$("Hello, WORLD")    ' hello, world
    IF LCASE$(a$) = "yes" THEN PRINT "ok"

See also [UCASE$](UCASE$), [LTRIM$](LTRIM$)
