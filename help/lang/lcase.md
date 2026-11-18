# LCASE$ Function
keys: LCASE$

Syntax
    LCASE$(string)

Returns string with every capital letter made lowercase.

See also [UCASE$](UCASE$), [LTRIM$](LTRIM$)

## Details
Only the letters A to Z change. Other characters are unchanged.

## Example
    PRINT LCASE$("Hello, WORLD")    ' hello, world
    IF LCASE$(a$) = "yes" THEN PRINT "ok"
