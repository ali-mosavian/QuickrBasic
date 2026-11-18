# GOTO Statement
keys: GOTO

Syntax
    GOTO {label | linenumber}

GOTO continues at the given label or line number. The target must be
in the same SUB, FUNCTION or module code as the GOTO. A label is a
name followed by a colon at the start of a line; a line number may
be used on its own. A missing target is Label not defined.

Jumping into a loop or an IF block from outside is poor style and
leaves loops confused. Prefer [DO...LOOP](DO), [EXIT](EXIT) and
[SELECT CASE](SELECT CASE).

## Example
    n = 0
    again:
    n = n + 1
    IF n < 3 THEN GOTO again
    PRINT n

See also [GOSUB](GOSUB), [ON GOTO](ON GOTO)
