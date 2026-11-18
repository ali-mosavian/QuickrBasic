# EXIT not within matching block
keys: EXIT not within matching block, error 234

Error 234

EXIT names a block kind you are not inside.

See also: [EXIT](EXIT), [FOR](FOR), [DO](DO)

## Details

Cause: [EXIT](EXIT) names a kind of block you are not inside: EXIT FOR
outside a FOR, EXIT DO outside a DO, EXIT SUB outside a SUB, EXIT
FUNCTION outside a FUNCTION.

Remedy: Move the EXIT into a block of that kind, or change it to the
kind you are in.

EXIT FOR, EXIT DO, EXIT SUB and EXIT FUNCTION each need an enclosing
block of that kind.

## Example
    DO
      EXIT FOR           ' error 234: inside DO, not FOR
    LOOP
    ' fix
    DO
      EXIT DO
    LOOP
