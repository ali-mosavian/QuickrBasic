# INPUT and LINE INPUT Statements
keys: INPUT, LINE INPUT, INPUT #, LINE INPUT #

Syntax
    INPUT [;] ["prompt"{; | ,}] variablelist
    LINE INPUT [;] ["prompt";] stringvariable
    INPUT #filenumber, variablelist
    LINE INPUT #filenumber, stringvariable

INPUT reads one value per variable. LINE INPUT reads a whole line
into one string variable. Either can read from the keyboard or a file.

See also [INPUT$](INPUT$), [INKEY$](INKEY$), [OPEN](OPEN),
[WRITE](WRITE), [files](files)

## Details
INPUT reads one value per variable, the values separated by commas.
After a prompt, a semicolon adds "? " and a comma adds nothing. A
leading semicolon keeps the cursor on the line after Enter.

Typing the wrong kind of value, or too few or too many values, shows
"Redo from start" and asks again. From a file, INPUT # reads the same
comma-separated form, and a quoted string may contain commas.

LINE INPUT reads the whole line, commas and quotes included, into one
string variable. From the keyboard it takes at most 255 characters.
The end of line is not stored.

## Example
    INPUT "Name, age"; n$, a
    PRINT n$; " is"; a      ' Ann is 31  (typed: Ann, 31)
    LINE INPUT "Sentence: "; s$
    PRINT LEN(s$)
