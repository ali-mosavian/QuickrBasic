# EXIT Statement
keys: EXIT

Syntax
    EXIT DO
    EXIT FOR [counter]
    EXIT WHILE
    EXIT SUB
    EXIT FUNCTION
    EXIT DEF

EXIT leaves the block named at once, and continues after it.

See also [CONTINUE](CONTINUE), [DO...LOOP](DO), [FOR...NEXT](FOR),
[WHILE...WEND](WHILE), [END](END)

## Details
EXIT leaves the block named at once, and continues after it:
the innermost [DO...LOOP](DO), [FOR...NEXT](FOR) or
[WHILE...WEND](WHILE), the current [SUB](SUB) or
[FUNCTION](FUNCTION), or a [DEF FN](DEF FN) block.

The loop left is the innermost of the kind named, through loops of
other kinds: EXIT DO inside a FOR inside a DO leaves the DO. A counter
after EXIT FOR names an outer FOR, as NEXT names it. An EXIT outside
the matching block is a compile-time error.

EXIT WHILE and EXIT FOR counter are QuickrBasic's.

In a FUNCTION, set the result before EXIT FUNCTION.

## Example
    FOR y = 1 TO 10
      FOR x = 1 TO 10
        IF x * y > 50 THEN EXIT FOR y    ' both loops
      NEXT
    NEXT
    PRINT x; y                           ' 6  9
