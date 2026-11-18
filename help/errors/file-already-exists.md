# File already exists
keys: File already exists, error 58

Error 58

NAME's new name is already in use.

See also: [NAME](NAME), [KILL](KILL)

## Details

Cause: [NAME](NAME) gives a new name that is already taken.

Remedy: Pick another name, or [KILL](KILL) the old file first.

The old file is untouched.

Trap it with ON ERROR; ERR returns the number.

## Example
    NAME "A.TXT" AS "B.TXT"   ' error 58 if B.TXT exists
    KILL "B.TXT"               ' fix: remove it first
    NAME "A.TXT" AS "B.TXT"
