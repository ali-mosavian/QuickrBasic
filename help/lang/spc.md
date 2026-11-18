# SPC Function
keys: SPC

Syntax
    PRINT SPC(n) ...
    LPRINT SPC(n) ...

Writes n blanks at the current position, inside PRINT or LPRINT.
Unlike TAB it counts from where the cursor is, not from the left
edge. It is not an expression and cannot be assigned to a variable;
use SPACE$ for that.

## Example
    PRINT "A"; SPC(3); "B"              ' A   B

See also [TAB](TAB), [SPACE$](SPACE$), [PRINT](PRINT)
