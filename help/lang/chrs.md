# CHR$ Function
keys: CHR$

Syntax
    CHR$(code)

Returns the one-character string with character code code, 0 to
255. A code outside that range gives Illegal function call. CHR$ is
the inverse of [ASC](ASC).

See also [ASC](ASC), [ASCII](ASCII), [STRING$](STRING$)

## Details
Returns the one-character string with character code code, 0 to
255. A code outside that range gives Illegal function call. CHR$ is
the inverse of [ASC](ASC).

Useful codes: 7 beep, 8 backspace, 9 tab, 13 carriage return,
10 line feed, 34 the double quote.

CHR$(0) is a real character, not the end of a string.

## Example
    PRINT CHR$(34); "quoted"; CHR$(34)
    PRINT CHR$(65)             ' A
