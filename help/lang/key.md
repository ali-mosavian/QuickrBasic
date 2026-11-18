# KEY Statement
keys: KEY, KEY ON, KEY OFF, KEY LIST

Syntax
    KEY n, string
    KEY ON | OFF | LIST

KEY n, string defines a trap key for [ON KEY](ON KEY). KEY ON, OFF
and LIST do nothing here.

See also [KEY(n)](key-n), [ON KEY](ON KEY), [scan codes](scan codes)

## Details
KEY n, string defines a trap key for [ON KEY](ON KEY), n from 15 to
25. string is two characters: CHR$(shiftflags) + CHR$(scancode), the
shift flags being the sum of 1 or 2 (Shift), 4 (Ctrl), 8 (Alt), 32
(Num Lock) and 64 (Caps Lock), or 0 for none.

The function-key numbers 1 to 10, 30 and 31 are accepted, but their
text is not changed: QuickrBasic has no function-key line, so KEY ON,
KEY OFF and KEY LIST also do nothing. Any other n is an Illegal
function call.

## Example
    KEY 15, CHR$(4) + CHR$(30)      ' Ctrl+A (scan code 30)
    KEY(15) ON
    ON KEY(15) GOSUB Hit
    DO: LOOP UNTIL INKEY$ = "q"
    END
    Hit: PRINT "Ctrl+A": RETURN
