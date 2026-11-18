# SUB/FUNCTION without END SUB/FUNCTION
keys: ~SUB/FUNCTION without END SUB/FUNCTION, ~END SUB/FUNCTION without SUB/FUNCTION, error 232, error 233

Errors 232 and 233

## Cause

A [SUB](SUB) or [FUNCTION](FUNCTION) is never closed, or an END SUB or
END FUNCTION appears with no procedure open.

## Remedy

End every procedure with END SUB or END FUNCTION, matching its kind.
Remove a stray END line.

See also: [SUB](SUB), [FUNCTION](FUNCTION), [DECLARE](DECLARE)
