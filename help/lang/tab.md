# TAB Function
keys: TAB

Syntax
    PRINT TAB(column) ...
    LPRINT TAB(column) ...

Moves the print position to column, counted from 1, inside PRINT or
LPRINT.

See also [SPC](SPC), [SPACE$](SPACE$), [PRINT](PRINT),
[LOCATE](LOCATE)

## Details

Moves the print position to column, counted from 1, inside PRINT or
LPRINT. If the position is already past column, the output goes to
column on the next line. A column under 1 counts as 1.

TAB moves to a column, not by a number of columns: to move by n use
SPC(n). TAB is not usable outside PRINT or LPRINT.

## Example
    PRINT TAB(5); "Name"; TAB(20); "Score"
    ' Name at column 5, Score at column 20
