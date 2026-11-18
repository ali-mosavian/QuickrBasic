# DATA Statement
keys: DATA

Syntax
    DATA constant[, constant]...

DATA stores constants in the program for READ to fetch, in order, from
the first DATA line to the last. A DATA line is only read, never run;
it may sit anywhere in the module.

A constant is a number or a string. A string needs quotes only when it
holds a comma, a colon, or leading or trailing blanks. Two commas in a
row, or a comma at the end, give an empty item: 0 for a numeric
variable, "" for a string. READ past the last item is an error
(Out of DATA).

## Example
    FOR i = 1 TO 3
      READ name$, age
      PRINT name$; " is"; age
    NEXT
    DATA Ann, 31, Bo, 27, "Cy, Jr.", 45

See also [READ](READ), [RESTORE](RESTORE)
