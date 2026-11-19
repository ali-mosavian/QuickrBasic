# WRITE Statement
keys: WRITE

Syntax
    WRITE [#filenumber,] [expressionlist]

Writes values in a form INPUT can read back: items separated by
commas, strings in quotes, numbers without the leading blank PRINT
adds.

See also [PRINT](PRINT), [INPUT](INPUT), [OPEN](OPEN), [files](files)

## Details

Writes values in a form INPUT can read back: items separated by
commas, strings in quotes, numbers without the leading blank PRINT
adds. The line ends with a newline. Without filenumber it goes to the
screen; with no list, it writes an empty line.

PRINT # writes values for people to read; WRITE # is for programs to
read back with INPUT #. A string that holds a comma survives the round
trip.

## Example
    OPEN "P.TXT" FOR OUTPUT AS #1
    WRITE #1, "Ann", 31, 1.5
    CLOSE #1                            ' file: "Ann",31,1.5
    WRITE "x", 2                        ' screen: "x",2
