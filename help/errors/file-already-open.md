# File already open
keys: File already open, error 55

Error 55

## Cause

OPEN names a file number already in use, or a file already open; or
KILL or NAME hits an open file.

## Remedy

CLOSE the file first, or take a free number with [FREEFILE](FREEFILE).

See also: [OPEN](OPEN), [CLOSE](CLOSE), [FREEFILE](FREEFILE)
