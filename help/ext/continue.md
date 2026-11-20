# CONTINUE Statement
keys: CONTINUE, CONTINUE FOR, CONTINUE DO, CONTINUE WHILE

Syntax
    CONTINUE FOR [counter]
    CONTINUE DO
    CONTINUE WHILE

CONTINUE skips the rest of a loop's body and goes on with its next
round. QuickrBasic adds it to QBasic.

See also [EXIT](EXIT), [FOR...NEXT](FOR), [DO...LOOP](DO),
[WHILE...WEND](WHILE)

## Details
CONTINUE goes to the loop's closing statement: NEXT steps the counter
and tests it, LOOP and WEND test their condition, as if the body had
run to its end.

The loop is the innermost of the kind named, through loops of other
kinds. A counter after CONTINUE FOR names an outer FOR, as NEXT names
it. A CONTINUE outside such a loop is a compile-time error.

CONTINUE is a keyword only at the start of a statement, before FOR, DO
or WHILE. Anywhere else it is an ordinary name, so a program with a
variable, label or SUB called continue runs as before.

## Example
    FOR i = 1 TO 10
      IF i MOD 2 = 0 THEN CONTINUE FOR
      PRINT i;                           ' 1  3  5  7  9
    NEXT
