# CONTINUE not within matching block
keys: CONTINUE not within matching block, error 236

Error 236

CONTINUE names a loop you are not inside.

See also: [CONTINUE](CONTINUE), [FOR](FOR), [DO](DO), [WHILE](WHILE)

## Details

Cause: [CONTINUE](CONTINUE) names a kind of loop you are not inside:
CONTINUE FOR outside a FOR, CONTINUE DO outside a DO, CONTINUE WHILE
outside a WHILE. Or the counter after CONTINUE FOR is not the counter
of any FOR open there.

Remedy: Move the CONTINUE into a loop of that kind, change it to the
kind you are in, or name the counter of a loop the line is inside.

## Example
    DO
      CONTINUE FOR       ' error 236: inside DO, not FOR
    LOOP
    ' fix
    DO
      CONTINUE DO
    LOOP UNTIL INKEY$ <> ""
