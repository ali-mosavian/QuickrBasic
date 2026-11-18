# IF...THEN...ELSE Statement
keys: IF, END IF, ELSE, ELSEIF, IF...THEN...ELSE, block IF

Syntax
    IF condition THEN statements [ELSE statements]

    IF condition THEN
        statements
    [ELSEIF condition THEN
        statements]...
    [ELSE
        statements]
    END IF

Runs statements depending on a condition, on one line or as a block.

See also [SELECT CASE](SELECT CASE), [operators](operators),
[expressions](expressions)

## Details
condition is any numeric expression: zero is false, anything else is
true. Comparisons give -1 (true) or 0 (false).

One-line form: runs the statements after THEN, or those after ELSE,
and ends at the end of the line. A line number or a label after THEN
or ELSE means GOTO it.

Block form: THEN is the last thing on its line. The tests are made
in order and the first true one runs its block; ELSE runs when none
was true. After a block, execution goes on after END IF. Blocks may
be nested, and each needs its own END IF.

## Example
    INPUT "Score"; s
    IF s >= 90 THEN
        PRINT "A"
    ELSEIF s >= 50 THEN
        PRINT "Pass"
    ELSE
        PRINT "Retry"
    END IF
    IF s = 0 THEN PRINT "zero" ELSE PRINT "nonzero"
