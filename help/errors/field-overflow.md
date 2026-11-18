# FIELD overflow / FIELD statement active
keys: FIELD overflow, FIELD statement active, error 50, error 56

Errors 50 and 56

## Cause

The fields of a [FIELD](FIELD) add up to more than the record length
of the file, or a GET or PUT with a variable was tried while FIELD is
active.

## Remedy

Open with a larger LEN, or shrink the fields. Use either FIELD or
variable I/O on a file, not both.

See also: [FIELD](FIELD), [OPEN](OPEN)
