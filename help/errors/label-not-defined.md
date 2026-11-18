# Label not defined
keys: Label not defined, error 8

Error 8

## Cause

A [GOTO](GOTO), [GOSUB](GOSUB), [ON ERROR](ON ERROR) or similar names
a line label or number that does not exist in this module or
procedure.

## Remedy

Define the label as `name:` on its own line, or fix the spelling. A
jump cannot leave a SUB or FUNCTION.

See also: [GOTO](GOTO), [GOSUB](GOSUB), [RESUME](RESUME)
