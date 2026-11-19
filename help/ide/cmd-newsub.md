# New SUB Command
keys: New SUB Command

Edit, New SUB...

Makes a new SUB in its own window.

See also [New FUNCTION Command](New FUNCTION Command), [SUB](SUB).

## Details
Asks for a name in the [New SUB Dialog](New SUB Dialog) and opens a
fresh window holding `SUB name` and `END SUB`. Type the body between
them. The SUB is listed by [SUBs Command](SUBs Command). Cancel, or an
empty name, makes nothing.

## Example
    Alt+E, S       the name box opens
    type  Greet    then Enter
    The window shows:
        SUB Greet
        END SUB
    Type between them:   PRINT "Hello"
    F2 lists Greet; call it from the main module with  Greet
