# INKEY$ Function
keys: INKEY$

Syntax
    INKEY$

Returns the next key from the keyboard buffer, or "" at once when no
key is waiting.

See also [INPUT$](INPUT$), [INPUT](INPUT), [scan codes](scan codes)

## Details
INKEY$ takes one key from the keyboard buffer. It never waits and
never echoes. Poll it in a loop to wait for a key.

An ordinary key gives a one-character string. An extended key (arrows,
function keys, Home, ...) gives two characters: CHR$(0) followed by
the key's [scan code](scan codes).

## Example
    DO
        k$ = INKEY$
    LOOP WHILE k$ = ""
    IF LEN(k$) = 2 THEN
        PRINT "extended key, code"; ASC(RIGHT$(k$, 1))
    ELSE
        PRINT "key "; k$
    END IF
    ' pressing Up arrow prints: extended key, code 72
