# FILEATTR Function
keys: FILEATTR

Syntax
    FILEATTR(filenumber, attribute)

FILEATTR returns the mode or the DOS handle of an open file.

See also [OPEN](OPEN), [FREEFILE](FREEFILE)

## Details
FILEATTR returns facts about an open file. Attribute selects which:

    1   the mode it was opened in
          1 INPUT   2 OUTPUT   4 RANDOM   8 APPEND   32 BINARY
    2   the DOS file handle

Any other attribute, or a closed file, is an error (Bad file name or
number for the number, Illegal function call for the attribute).

## Example
    OPEN "t.txt" FOR OUTPUT AS #1
    PRINT FILEATTR(1, 1)      ' 2
    CLOSE #1
