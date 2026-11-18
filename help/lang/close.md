# CLOSE Statement
keys: CLOSE

Syntax
    CLOSE [[#]n [, [#]n] ...]

Ends the program's use of the files with the given numbers, writing
out whatever is buffered. CLOSE alone closes every open file. The
numbers can then be reused by OPEN.

See also [OPEN](OPEN), [FREEFILE](FREEFILE), [RESET](RESET),
[files](files)

## Details
Ends the program's use of the files with the given numbers, writing
out whatever is buffered. CLOSE alone closes every open file. The
numbers can then be reused by OPEN.

Closing a number that is not open is not an error. Data written
with PRINT # may not reach the disk until the file is closed, so
close files you wrote before the program ends in an unusual way.
END and SYSTEM close them for you.

## Example
    OPEN "OUT.TXT" FOR OUTPUT AS #1
    PRINT #1, "done"
    CLOSE #1
