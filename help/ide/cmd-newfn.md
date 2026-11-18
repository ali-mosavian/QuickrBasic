# New FUNCTION Command
keys: New FUNCTION Command

Edit, New FUNCTION...

Makes a new FUNCTION in its own window.

See also [FUNCTION](FUNCTION), [New SUB Command](New SUB Command).

## Details
Like [New SUB Command](New SUB Command), for a FUNCTION: the window
starts with `FUNCTION name` and `END FUNCTION`. The name box is the
[New FUNCTION Dialog](New FUNCTION Dialog). Cancel or an empty name
makes nothing. Type the body between the two lines, and set the
result by assigning to the name.

## Example
    Alt+E, F       the name box opens
    type  Twice    then Enter
    The window shows:
        FUNCTION Twice
        END FUNCTION
    Type between them:   Twice = 2 * 21
