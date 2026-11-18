# Label not defined
keys: Label not defined, error 8

Error 8

A jump names a label that does not exist here.

See also: [GOTO](GOTO), [GOSUB](GOSUB), [RESUME](RESUME)

## Details

Cause: A [GOTO](GOTO), [GOSUB](GOSUB), [ON ERROR](ON ERROR) or similar
names a line label or number that does not exist in this module or
procedure.

Remedy: Define the label as `name:` on its own line, or fix the
spelling. A jump cannot leave a SUB or FUNCTION.

A label is per module or procedure; a jump cannot leave a SUB or
FUNCTION.

## Example
    GOTO done          ' error 8: no label "done"
    GOTO done            ' fixed by adding the label
    PRINT "skipped"
    done:
    PRINT "end"
