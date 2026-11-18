# WIDTH Statement
keys: WIDTH

Syntax
    WIDTH [columns] [, rows]
    WIDTH LPRINT columns
    WIDTH #filenumber, columns
    WIDTH device$, columns

The first form sets the screen's text size. columns is 40 or 80;
rows is 25, 30, 43, 50 or 60, as the adapter and SCREEN mode allow.
Changing it clears the screen.

The others set how many characters a line of output holds before the
next one starts: for the printer, an open file or a device name
(such as "LPT1:" or "SCRN:"). columns is 1 to 255; 255 means no
wrapping.

## Example
    WIDTH 80, 50
    OPEN "OUT.TXT" FOR OUTPUT AS #1
    WIDTH #1, 40

See also [SCREEN](SCREEN), [VIEW PRINT](VIEW PRINT), [PRINT](PRINT),
[LPRINT](LPRINT), [OPEN](OPEN)
