# INKEY$ Function
keys: INKEY$

Syntax
    INKEY$

Takes one key from the keyboard buffer and returns it, or returns ""
at once when no key is waiting. It never waits and never echoes.

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

See also [INPUT$](INPUT$), [INPUT](INPUT), [scan codes](scan codes)
