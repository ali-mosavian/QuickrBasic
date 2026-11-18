# ON expression GOTO, GOSUB
keys: ON GOTO, ON GOSUB, ON expression GOTO

Syntax
    ON expression GOTO target1[, target2 ...]
    ON expression GOSUB target1[, target2 ...]

Jumps to the nth target, where n is the value of expression.

See also [RETURN](RETURN), [GOSUB](GOSUB), [SELECT CASE](SELECT CASE)

## Details
expression is rounded to an integer n. If n is 1, control goes to the
first target, 2 to the second, and so on. Targets are line numbers or
labels.

If n is 0 or beyond the last target, nothing happens and the program
continues below. A negative n, or one over 255, is "Illegal function
call".

With GOSUB, the called code ends with [RETURN](RETURN), which comes
back to the statement after the ON.

SELECT CASE does the same with readable cases.

## Example
    INPUT "Choice (1-3)"; c
    ON c GOSUB One, Two, Three
    END
    One: PRINT "one": RETURN
    Two: PRINT "two": RETURN
    Three: PRINT "three": RETURN
    ' typing 2 prints: two
