# PRINT # and PRINT # USING
keys: PRINT #, PRINT # USING, PRINT#

Syntax
    PRINT #n, [expressionlist][{; | ,}]
    PRINT #n, USING format$; expressionlist[{; | ,}]

Writes to the file or device opened as #n, with the same rules as
[PRINT](PRINT) and [PRINT USING](PRINT USING): the text is exactly
what the screen would show, ending in a line break unless the list
ends in ; or ,.

To read it back, items must be separated so INPUT # can tell them
apart. Numbers are, by their spaces; strings are not, so write them
with [WRITE #](WRITE) or add commas yourself:

    PRINT #1, a$; ","; b$

The file must be open FOR OUTPUT or APPEND (or RANDOM or BINARY).
Otherwise: "Bad file mode".

## Example
    OPEN "OUT.TXT" FOR OUTPUT AS #1
    PRINT #1, "Score:"; 1200
    PRINT #1, USING "###.##"; 3.14159
    CLOSE #1

See also [OPEN](OPEN), [WRITE #](WRITE), [INPUT #](INPUT #)
