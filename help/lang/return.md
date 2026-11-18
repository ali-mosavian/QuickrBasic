# RETURN Statement
keys: RETURN

Syntax
    RETURN [{line | label}]

Ends a subroutine called with GOSUB (or ON ... GOSUB, or an event) and
goes back to the statement after the call. With a line or label it
goes there instead; the call is forgotten either way.

See also [GOSUB](GOSUB), [ON GOTO](ON GOTO), [ON event](ON event)

## Details

Ends a subroutine called with GOSUB (or ON ... GOSUB, or an event) and
goes back to the statement after the call. With a line or label it
goes there instead; the call is forgotten either way.

RETURN with no waiting GOSUB is "RETURN without GOSUB". A RETURN
with a target cannot leave a SUB or FUNCTION.

Event handlers (ON KEY, ON TIMER and so on) end with RETURN too; the
program resumes where the event interrupted it.

## Example
    GOSUB Hello
    PRINT "back"                   ' hello, then back
    END
    Hello: PRINT "hello": RETURN
