# WRITE Statement
keys: WRITE

Syntax
    WRITE [#filenumber,] [expressionlist]

Writes values in a form INPUT can read back: items separated by
commas, strings in quotes, numbers without the leading blank PRINT
adds. The line ends with a newline. Without filenumber it goes to the
screen; with no list, it writes an empty line.

## Example
    OPEN "P.TXT" FOR OUTPUT AS #1
    WRITE #1, "Ann", 31, 1.5
    CLOSE #1                            ' file: "Ann",31,1.5

See also [PRINT](PRINT), [INPUT](INPUT), [OPEN](OPEN), [files](files)
