# EXIT Statement
keys: EXIT

Syntax
    EXIT DO
    EXIT FOR
    EXIT SUB
    EXIT FUNCTION
    EXIT DEF

EXIT leaves the block named at once, and continues after it:
the innermost [DO...LOOP](DO) or [FOR...NEXT](FOR), the current
[SUB](SUB) or [FUNCTION](FUNCTION), or a [DEF FN](DEF FN) block.
In a nested loop of the same kind only the innermost ends. An EXIT
outside the matching block is a compile-time error.

See also [DO...LOOP](DO), [FOR...NEXT](FOR), [END](END)

## Details
EXIT leaves the block named at once, and continues after it:
the innermost [DO...LOOP](DO) or [FOR...NEXT](FOR), the current
[SUB](SUB) or [FUNCTION](FUNCTION), or a [DEF FN](DEF FN) block.
In a nested loop of the same kind only the innermost ends. An EXIT
outside the matching block is a compile-time error.

In a FUNCTION, set the result before EXIT FUNCTION.

## Example
    FOR i = 1 TO 100
      IF i * i > 50 THEN EXIT FOR
    NEXT
    PRINT i
