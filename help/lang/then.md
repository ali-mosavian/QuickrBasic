# THEN Keyword
keys: THEN

THEN follows the condition in an IF or ELSEIF. After it comes either
a statement list on the same line, or a line end and a block that
runs to ELSE, ELSEIF or END IF. A line number or label right after
THEN means GOTO it.

## Example
    IF n% > 9 THEN PRINT "big" ELSE PRINT "small"
    IF n% = 0 THEN 100

See also [IF](IF), [GOTO](GOTO)
