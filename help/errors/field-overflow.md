# FIELD overflow / FIELD statement active
keys: FIELD overflow, FIELD statement active, error 50, error 56

Errors 50 and 56

FIELD widths exceed the record length, or FIELD and variable I/O are
mixed.

See also: [FIELD](FIELD), [OPEN](OPEN)

## Details

Cause: The fields of a [FIELD](FIELD) add up to more than the record
length of the file, or a GET or PUT with a variable was tried while
FIELD is active.

Remedy: Open with a larger LEN, or shrink the fields. Use either FIELD
or variable I/O on a file, not both.

Error 50 FIELD overflow; error 56 FIELD statement active.

Trap it with ON ERROR; ERR returns the number.

## Example
    OPEN "R.DAT" FOR RANDOM AS #1 LEN = 10
    FIELD #1, 8 AS a$, 8 AS b$   ' error 50: 16 > 10
    FIELD #1, 5 AS a$, 5 AS b$   ' fixed
    CLOSE #1
