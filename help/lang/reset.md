# RESET Statement
keys: RESET

Syntax
    RESET

Closes every open file and device, writing out what is buffered.

See also [CLOSE](CLOSE), [OPEN](OPEN)

## Details

Closes every open file and device, writing out what is buffered.
Same as CLOSE with no list.

RESET is the same as CLOSE with no file list. Only files and devices
opened by the program are closed; variables keep their values.

## Example
    OPEN "A.TXT" FOR OUTPUT AS #1
    OPEN "B.TXT" FOR OUTPUT AS #2
    RESET                          ' both files are closed
    OPEN "A.TXT" FOR INPUT AS #1   ' #1 is free again
