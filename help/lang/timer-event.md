# TIMER Statements, ON TIMER
keys: TIMER event, ON TIMER, TIMER ON, TIMER OFF, TIMER STOP

Syntax
    ON TIMER(seconds) GOSUB line
    TIMER ON | OFF | STOP

ON TIMER names the handler and the period, 1 to 86400 seconds. TIMER
ON starts the trap: every period the program jumps to line, and
RETURN resumes where it left off. TIMER OFF ends it. TIMER STOP holds
events, which fire at the next TIMER ON.

Events are noticed between statements, so a long statement delays
the handler. The handler is not re-entered while it runs.

## Example
    ON TIMER(1) GOSUB Tick
    TIMER ON
    DO: LOOP UNTIL INKEY$ <> ""
    END
    Tick:
        LOCATE 1, 1: PRINT TIME$
        RETURN

See also [TIMER](TIMER), [ON KEY](KEY), [SLEEP](SLEEP),
[GOSUB](GOSUB)
