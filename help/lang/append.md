# APPEND Mode
keys: APPEND

Syntax
    OPEN file$ FOR APPEND AS #n

Opens a sequential file for writing, positioned at its end, so what
the program writes is added after the existing text. A file that
does not exist is created. Write with PRINT #n or WRITE #n.

Reading from an APPEND file fails with Bad file mode.

## Example
    OPEN "LOG.TXT" FOR APPEND AS #1
    PRINT #1, TIME$; " started"
    CLOSE #1

See also [OPEN](OPEN), [PRINT](PRINT), [CLOSE](CLOSE), [files](files)
