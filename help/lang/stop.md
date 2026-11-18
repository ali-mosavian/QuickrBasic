# STOP Statement
keys: STOP

Syntax
    STOP

Halts the program as if a breakpoint were there. In the IDE you can
inspect variables, then continue (F5) or step. Unlike END, STOP does
not close files.

STOP also suspends event trapping (ON KEY, ON TIMER, ...) until the
program continues.

## Example
    IF total% < 0 THEN STOP

See also [END](END), [SYSTEM](SYSTEM), [Debug Menu](Debug Menu)
