# CONST Statement
keys: CONST

Syntax
    CONST name = expression [, name = expression] ...

Gives a name to a value that does not change. The expression may use
literals, other constants and operators, but no variables or
function calls other than the built-in ones. A name ending in $
holds a string; the type of any other is that of its value, unless
the name has a suffix.

See also [DIM](DIM), [data types](data types)

## Details
Gives a name to a value that does not change. The expression may use
literals, other constants and operators, but no variables or
function calls other than the built-in ones. A name ending in $
holds a string; the type of any other is that of its value, unless
the name has a suffix.

A constant cannot be assigned to afterwards. A constant declared in
a SUB or FUNCTION exists only there; one declared at the top of the
program is visible everywhere.

## Example
    CONST pi = 3.14159, radius = 4
    CONST area = pi * radius * radius
    CONST title$ = "Circle"
    PRINT title$; area       ' Circle 50.26544
