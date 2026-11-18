# STOP Statement
keys: STOP

Syntax
    STOP

Halts the program as if a breakpoint were there. In the IDE you can
inspect variables, then continue (F5) or step. Unlike END, STOP does
not close files.

See also [END](END), [SYSTEM](SYSTEM), [Debug Menu](Debug Menu)

## Details

Halts the program as if a breakpoint were there. In the IDE you can
inspect variables, then continue (F5) or step. Unlike END, STOP does
not close files.

STOP also suspends event trapping (ON KEY, ON TIMER, ...) until the
program continues.

STOP is a debugging aid; take it out of a finished program. F5 carries
on after it.

## Example
    total% = -1
    IF total% < 0 THEN STOP         ' halts here in the IDE
    PRINT "after F5"                ' runs once you continue
