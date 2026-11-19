# Permission denied
keys: Permission denied, error 70

Error 70

The access asked for is not allowed.

See also: [OPEN](OPEN)

## Details

Cause: The file is read-only, locked by another program, or the access
asked for is not allowed.

Remedy: Open it for reading only, or change its attributes outside the
program.

Typical causes: a read-only file, or a file locked by another program.

Trap it with ON ERROR; ERR returns the number.

## Example
    OPEN "RO.TXT" FOR OUTPUT AS #1   ' error 70 if read-only
    OPEN "RO.TXT" FOR INPUT AS #1    ' fixed: read only
    CLOSE #1
