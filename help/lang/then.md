# THEN Keyword
keys: THEN

THEN follows the condition in an IF or ELSEIF.

See also [IF](IF), [GOTO](GOTO)

## Details

THEN follows the condition in an IF or ELSEIF. After it comes either
a statement list on the same line, or a line end and a block that
runs to ELSE, ELSEIF or END IF. A line number or label right after
THEN means GOTO it.

A one-line IF may have a statement list, separated by colons, after
THEN and after ELSE. A block IF must have nothing after THEN except a
comment. THEN is optional before GOTO: IF a% THEN GOTO 100 is the same
as IF a% GOTO 100.

## Example
    n% = 12
    IF n% > 9 THEN PRINT "big" ELSE PRINT "small"    ' big
    IF n% = 0 THEN 100
