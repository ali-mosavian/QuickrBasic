# BYVAL Keyword
keys: BYVAL

Syntax
    DECLARE SUB name (BYVAL param AS type)

Passes an argument by value: the procedure gets a copy, so
changing the parameter leaves the caller's variable alone. Normally
a variable is passed by reference and the procedure can change it.

See also [SUB](SUB), [DECLARE](DECLARE), [CALL](CALL)

## Details
Passes an argument by value: the procedure gets a copy, so
changing the parameter leaves the caller's variable alone. Normally
a variable is passed by reference and the procedure can change it.

BYVAL in the DECLARE and SUB lines makes every call copy. In a
call, parenthesizing an argument, name (x), has the same effect.

## Example
    DECLARE SUB Bump (BYVAL n AS INTEGER)
    x% = 5: Bump x%
    PRINT x%                ' still 5
    SUB Bump (BYVAL n AS INTEGER)
        n = n + 1
    END SUB
