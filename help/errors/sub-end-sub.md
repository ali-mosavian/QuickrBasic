# SUB/FUNCTION without END SUB/FUNCTION
keys: ~SUB/FUNCTION without END SUB/FUNCTION, ~END SUB/FUNCTION without SUB/FUNCTION, error 232, error 233

Errors 232 and 233

A SUB or FUNCTION is not closed, or an END SUB/FUNCTION is stray.

See also: [SUB](SUB), [FUNCTION](FUNCTION), [DECLARE](DECLARE)

## Details

Cause: A [SUB](SUB) or [FUNCTION](FUNCTION) is never closed, or an END
SUB or END FUNCTION appears with no procedure open.

Remedy: End every procedure with END SUB or END FUNCTION, matching its
kind. Remove a stray END line.

Error 232 and 233.

## Example
    SUB T
      PRINT "x"
    ' error 232: END SUB missing
    SUB T
      PRINT "x"
    END SUB              ' fixed
