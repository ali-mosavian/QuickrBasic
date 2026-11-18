# Bad record length / Bad record number
keys: Bad record length, Bad record number, error 59, error 63

Errors 59 and 63

## Cause

A GET or PUT variable does not match the file's record length, or the
record number is zero or negative.

## Remedy

Make LEN in OPEN equal the size of the record variable. Record numbers
start at 1.

See also: [OPEN](OPEN), [GET](GET), [PUT](PUT), [LEN](LEN)
