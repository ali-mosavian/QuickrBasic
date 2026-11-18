# FOR...NEXT Statement
keys: FOR, NEXT, FOR...NEXT

Syntax
    FOR counter = start TO end [STEP increment]
      statements
    NEXT [counter[, counter]...]

FOR runs its block with counter going from start to end. Start, end
and increment are worked out once, on entry. The step defaults to 1;
a negative step counts down and ends when the counter falls below
end. The block is skipped if the counter starts already past end. A
zero step loops until the block exits it.

After a normal finish the counter holds the first value past end.
Leave early with [EXIT](EXIT) FOR. A name after NEXT must match the
loop; one NEXT may close several loops: NEXT j, i. Use a whole-number
counter for speed and to avoid rounding drift in fractional steps.

## Example
    FOR i = 10 TO 0 STEP -5
      PRINT i;
    NEXT
    ' 10 5 0

See also [DO...LOOP](DO), [WHILE...WEND](WHILE), [EXIT](EXIT)
