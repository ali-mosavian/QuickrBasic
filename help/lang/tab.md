# TAB Function
keys: TAB

Syntax
    PRINT TAB(column) ...
    LPRINT TAB(column) ...

Moves the print position to column, counted from 1, inside PRINT or
LPRINT. If the position is already past column, the output goes to
column on the next line. A column under 1 counts as 1.

## Example
    PRINT TAB(5); "Name"; TAB(20); "Score"

See also [SPC](SPC), [SPACE$](SPACE$), [PRINT](PRINT),
[LOCATE](LOCATE)
