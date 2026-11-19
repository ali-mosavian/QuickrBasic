# Identifier errors
keys: Identifier expected, ~Identifier expected /TAB, ~Identifier expected /SPC, Identifier too long, identifier cannot begin with underscore, Invalid character, Function name illegal, Name of subprogram illegal, ~Name of subprogram illegal /reserved

A name is not a legal identifier.

See also: [keyword lists](keyword lists), [SUB](SUB), [FUNCTION](FUNCTION)

## Details

Cause: A name is a reserved word, is over 40 characters, starts with
an underscore, carries a type suffix where none is allowed, or
contains an illegal character. TAB and SPC need `(` after them.

Remedy: Rename it: begin with a letter, use letters, digits and `.`,
and avoid keywords. Write `TAB(n)` and `SPC(n)`.

Names are at most 40 characters, start with a letter and may not be
reserved words. TAB and SPC need parentheses.

## Example
    PRINT TAB 5; "x"    ' error: ( expected
    PRINT TAB(5); "x"    ' fixed
