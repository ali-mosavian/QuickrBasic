# Rename across disks
keys: Rename across disks, error 74

Error 74

NAME was given paths on different drives.

See also: [NAME](NAME), [KILL](KILL)

## Details

Cause: [NAME](NAME) was given paths on different drives.

Remedy: Copy the file to the other drive, then [KILL](KILL) the
original.

NAME only moves within a drive.

Trap it with ON ERROR; ERR returns the number.

## Example
    NAME "A:F.TXT" AS "C:F.TXT"   ' error 74
    ' fix: copy it, then delete the original
    KILL "A:F.TXT"
