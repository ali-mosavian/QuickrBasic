# File already open
keys: File already open, error 55

Error 55

The file or file number is already in use.

See also: [OPEN](OPEN), [CLOSE](CLOSE), [FREEFILE](FREEFILE)

## Details

Cause: OPEN names a file number already in use, or a file already
open; or KILL or NAME hits an open file.

Remedy: CLOSE the file first, or take a free number with
[FREEFILE](FREEFILE).

KILL, NAME and a second OPEN of an open file all give it.

Trap it with ON ERROR; ERR returns the number.

## Example
    OPEN "A.TXT" FOR OUTPUT AS #1
    OPEN "B.TXT" FOR OUTPUT AS #1   ' error 55
    CLOSE #1
    n% = FREEFILE                   ' fixed: free number
    OPEN "B.TXT" FOR OUTPUT AS #n%
    CLOSE #n%
