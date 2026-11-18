# Duplicate label
keys: Duplicate label, error 33

Error 33

Two lines in one module or procedure have the same label.

See also: [GOTO](GOTO), [Label not defined](Label not defined)

## Details

Cause: Two lines in one module or procedure have the same label.

Remedy: Rename one of them.

Labels are per module; each SUB and FUNCTION has its own set.

## Example
    again:
    PRINT "a"
    again:               ' error 33
    ' fix: rename the second
    more:
    PRINT "b"
