# LPRINT and LPRINT USING Statements
keys: LPRINT, LPRINT USING

Syntax
    LPRINT [expressionlist] [{; | ,}]
    LPRINT USING format; expressionlist [{; | ,}]

Prints on the printer (LPT1) as [PRINT](PRINT) does on the screen.

See also [PRINT](PRINT), [PRINT USING](PRINT USING), [LPOS](LPOS),
[WIDTH](WIDTH)

## Details
The separators, TAB and SPC, and the [PRINT USING](PRINT USING)
formats are the same as for PRINT.

Output is sent line by line; a form feed is CHR$(12). WIDTH LPRINT n
sets the line width, 1 to 255 (255 means never wrap). With no printer
ready the statement fails with Device unavailable.

## Example
    LPRINT "Report"
    LPRINT USING "###.## units"; 3.14159    ' prints   3.14 units
    LPRINT CHR$(12);        ' eject the page
